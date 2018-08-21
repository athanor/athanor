#include "operators/opSetLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/opSetIndexInternal.h"
#include "operators/simpleOperator.hpp"
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;

void OpSetLit::evaluateImpl() {
    debug_code(assert(exprTriggers.empty()));
    numberUndefined = 0;
    mpark::visit(
        [&](auto& operands) {
            this->members.emplace<ExprRefVec<viewType(operands)>>();
            size_t startIndex = 0, endIndex = operands.size() - 1;
            while (startIndex <= endIndex) {
                auto& operand = operands[startIndex];
                operand->evaluate();
                if (operand->isUndefined()) {
                    numberUndefined++;
                    swap(operands[startIndex], operands[endIndex]);
                    --endIndex;
                } else if (memberHashes.count(getValueHash(operand->view()))) {
                    swap(operands[startIndex], operands[endIndex]);
                    this->addHash(getValueHash(operands[endIndex]->view()),
                                  endIndex);
                    --endIndex;
                } else {
                    this->addMember(operand);
                    this->addHash(getValueHash(operand->view()), startIndex);
                    ++startIndex;
                }
            }
        },
        operands);
    debug_code(assertValidHashes());
}

void OpSetLit::assertValidHashes() {
    debug_log(hashIndicesMap);
    size_t sumOfIndexSetSizes = 0;
    size_t numberUndefined = 0;
    bool success = true;
    mpark::visit(
        [&](auto& operands) {
            for (size_t i = 0; i < operands.size(); i++) {
                auto& operand = operands[i];
                if (operand->isUndefined()) {
                    ++numberUndefined;
                    continue;
                }
                HashType hash = getValueHash(operand->view());
                auto iter = hashIndicesMap.find(hash);
                if (iter == hashIndicesMap.end()) {
                    success = false;
                    cerr << "Error, hash " << hash << " of operand ";
                    operand->dumpState(cerr);
                    cerr << " maps to no index set.\n";
                    break;
                }
                if (!iter->second.count(i)) {
                    success = false;
                    operand->dumpState(cerr << "error: operand: ")
                        << " with hash " << hash
                        << " maps to an index set not containing the operand "
                           "index "
                        << i << endl;
                    cerr << iter->second << endl;
                    break;
                }
            }
            if (success) {
                for (const auto& hashIndicesPair : hashIndicesMap) {
                    sumOfIndexSetSizes += hashIndicesPair.second.size();
                }
                if (sumOfIndexSetSizes + numberUndefined != operands.size()) {
                    success = false;
                    cerr << "numberUndefined(" << numberUndefined
                         << ") + sumOfIndexSetSizes(" << sumOfIndexSetSizes
                         << ") should = operands.size(" << operands.size()
                         << ")\n";
                }
            }
            if (!success) {
                cerr << "HashIndicesMap: " << hashIndicesMap << endl;
                cerr << "operands:";
                for (auto& operand : operands) {
                    operand->dumpState(cerr << "\n");
                }
                cerr << endl;
                abort();
            }
        },
        operands);
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public OpSetLit::ExprTriggerBase,
      public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type View;
    HashType previousHash;
    using ExprTriggerBase::ExprTriggerBase;
    using ExprTriggerBase::index;
    using ExprTriggerBase::op;

    auto& getOperands() { return mpark::get<ExprRefVec<View>>(op->operands); }

    void adapterPossibleValueChange() {
        debug_log("Possible value change: index=" << index);
        debug_code(op->dumpState(cout) << endl);
        debug_code(op->assertValidHashes());
        if (index < op->numberElements()) {
            op->template notifyPossibleMemberChange<View>(index);
        }
        previousHash = getValueHash(getOperands()[index]->view());
        debug_log("previousHash=" << previousHash);
    }

    void swapOperands(size_t index1, size_t index2) {
        op->swapHashes<View>(index1, index2);
        swap(getOperands()[index1], getOperands()[index2]);
        swap(op->exprTriggers[index1], op->exprTriggers[index2]);
        swap(op->exprTriggers[index1]->index, op->exprTriggers[index2]->index);
        swap(static_pointer_cast<ExprTrigger<TriggerType>>(
                 op->exprTriggers[index1])
                 ->previousHash,
             static_pointer_cast<ExprTrigger<TriggerType>>(
                 op->exprTriggers[index2])
                 ->previousHash);
    }

    void swapWithOtherOperand(size_t newIndex) {
        swapOperands(this->index, newIndex);
    }

    auto manualRemoveOperandFromSetView(HashType newHash,
                                        bool operandIsStillDefined) {
        op->notifyPossibleSetValueChange();
        auto& members = op->getMembers<View>();
        op->memberHashes.erase(previousHash);
        op->cachedHashTotal -= mix(previousHash);
        members[index] = move(members.back());
        members.pop_back();
        op->notifyMemberRemoved(index, previousHash);
        auto otherOperandWithSameValue = op->removeHash(previousHash, index);
        if (operandIsStillDefined) {
            op->addHash(newHash, index);
        }
        swapWithOtherOperand(op->numberElements());
        return otherOperandWithSameValue;
    }

    void adapterValueChanged() {
        HashType newHash = getValueHash(getOperands()[index]->view());
        debug_log("hit, index=" << index << "\npreviousHash=" << previousHash
                                << "\nnewHash=" << newHash);
        ;
        debug_log("hashIndicesMap=" << op->hashIndicesMap);
        debug_code(op->dumpState(cout) << endl);

        if (newHash == previousHash) {
            debug_log("returning");
            return;
        }
        debug_code(assert(op->hashIndicesMap.count(previousHash)));
        bool operandBeingUsedInSet = index < op->numberElements();
        // if false, then another operand with the same value as this operands
        // old value is in the SetView, this operand was not being tracked as
        // part of the SetView
        bool newValueInSet = op->memberHashes.count(newHash);
        // whether or not the new value is already present in the set

        if (!operandBeingUsedInSet) {
            if (!newValueInSet) {
                // this operand was not in the set view, now it has a unique
                // value, it must be added
                op->addMemberAndNotify(getOperands()[index]);
                op->removeHash(previousHash, index);
                op->addHash(newHash, index);
                // now move the operand and trigger into the same index as the
                // newly added element to the set view
                size_t newIndex = op->numberElements() - 1;
                swapWithOtherOperand(newIndex);
            } else {
                // this operand was not being used in the set view, it still
                // can't be as its there is already an operand with its new
                // value
                op->removeHash(previousHash, index);
                op->addHash(newHash, index);
            }
        } else {
            if (!newValueInSet) {
                // operand was being used in the set view, it now has a new
                // value, notify the set of the new value
                op->template memberChangedAndNotify<View>(index, previousHash);
                auto otherOperandWithSameValue =
                    op->removeHash(previousHash, index);
                op->addHash(newHash, index);
                // however if other operands have the old value they must be
                // moved into the set view
                if (otherOperandWithSameValue.first) {
                    op->addMemberAndNotify(
                        getOperands()[otherOperandWithSameValue.second]);
                    swapOperands(op->numberElements() - 1,
                                 otherOperandWithSameValue.second);
                }
            } else {
                // operand is in set view and now changed to a value of another
                // operand in the set,  will remove this one
                auto otherOperandWithSameValue =
                    manualRemoveOperandFromSetView(newHash, true);
                ;
                // however, if other operand has the old value,  it's put back
                // in
                if (otherOperandWithSameValue.first) {
                    op->addMemberAndNotify(
                        getOperands()[otherOperandWithSameValue.second]);
                    debug_code(assert(index == op->numberElements() - 1));
                    swapWithOtherOperand(otherOperandWithSameValue.second);
                }
            }
        }
        previousHash = newHash;
    }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
            op->exprTriggers[index]));
        auto trigger = make_shared<ExprTrigger<TriggerType>>(op, index);
        op->getMembers<View>()[index]->addTrigger(trigger);
        op->exprTriggers[index] = trigger;
    }

    void adapterHasBecomeDefined() {
        HashType newHash = getValueHash(getOperands()[index]->view());
        bool newValueInSet = op->memberHashes.count(newHash);
        // whether or not the new value is already present in the set

        if (!newValueInSet) {
            // this operand was not in the set view, now it has a unique value,
            // it must be added
            op->addMemberAndNotify(getOperands()[index]);
            op->addHash(newHash, index);
            // now move the operand and trigger into the same index as the newly
            // added element to the set view
            swapWithOtherOperand(op->numberElements() - 1);
        } else {
            // this operand was not being used in the set view, it still can't
            // be as its there is already an operand with its new value
            op->addHash(newHash, index);
        }
    }

    void adapterHasBecomeUndefined() {
        bool operandBeingUsedInSet = index < op->numberElements();
        if (!operandBeingUsedInSet) {
            // this operand was not being used in the set, so parents don't
            // detect the change
            op->removeHash(previousHash, index);
        } else {
            // operand was being used in the set view, remove it
            auto otherOperandWithSameValue =
                manualRemoveOperandFromSetView(0, false);
            // if other operands had the old value, add them in
            if (otherOperandWithSameValue.first) {
                op->addMemberAndNotify(
                    getOperands()[otherOperandWithSameValue.second]);
                debug_code(assert(index == op->numberElements() - 1));
                swapWithOtherOperand(otherOperandWithSameValue.second);
            }
        }
    }
};
}  // namespace

OpSetLit::OpSetLit(OpSetLit&& other)
    : SetView(std::move(other)),
      operands(move(other.operands)),
      hashIndicesMap(move(other.hashIndicesMap)),
      exprTriggers(move(other.exprTriggers)),
      numberUndefined(move(other.numberUndefined)) {
    setTriggerParent(this, exprTriggers);
}

void OpSetLit::startTriggeringImpl() {
    if (exprTriggers.empty()) {
        mpark::visit(
            [&](auto& operands) {
                typedef typename AssociatedTriggerType<viewType(operands)>::type
                    TriggerType;
                for (size_t i = 0; i < operands.size(); i++) {
                    auto trigger =
                        make_shared<ExprTrigger<TriggerType>>(this, i);
                    operands[i]->addTrigger(trigger);
                    exprTriggers.emplace_back(move(trigger));
                    operands[i]->startTriggering();
                }
            },
            operands);
    }
}

void OpSetLit::stopTriggeringOnChildren() {
    if (!exprTriggers.empty()) {
        mpark::visit(
            [&](auto& operands) {
                typedef typename AssociatedTriggerType<viewType(operands)>::type
                    TriggerType;
                for (auto& trigger : exprTriggers) {
                    deleteTrigger(
                        static_pointer_cast<ExprTrigger<TriggerType>>(trigger));
                }
                exprTriggers.clear();
            },
            operands);
    }
}

void OpSetLit::stopTriggering() {
    if (!exprTriggers.empty()) {
        stopTriggeringOnChildren();
        mpark::visit(
            [&](auto& operands) {
                for (auto& operand : operands) {
                    operand->stopTriggering();
                }
            },
            operands);
    }
}

void OpSetLit::updateVarViolationsImpl(const ViolationContext& context,
                                       ViolationContainer& container) {
    mpark::visit(
        [&](auto& operands) {
            for (auto& operand : operands) {
                operand->updateVarViolations(context, container);
            }
        },
        operands);
}

ExprRef<SetView> OpSetLit::deepCopySelfForUnrollImpl(
    const ExprRef<SetView>&, const AnyIterRef& iterator) const {
    return mpark::visit(
        [&](auto& operands) {
            ExprRefVec<viewType(operands)> newOperands;
            ExprRefVec<viewType(operands)> newSetMembers;

            for (auto& operand : operands) {
                newOperands.emplace_back(
                    operand->deepCopySelfForUnroll(operand, iterator));
                if (newOperands.size() <= numberElements()) {
                    newSetMembers.emplace_back(newOperands.back());
                }
            }
            auto newOpSetLit = make_shared<OpSetLit>(move(newOperands));
            newOpSetLit->members = newSetMembers;
            newOpSetLit->memberHashes = memberHashes;
            newOpSetLit->cachedHashTotal = cachedHashTotal;
            newOpSetLit->hashIndicesMap = hashIndicesMap;
            newOpSetLit->numberUndefined = numberUndefined;
            return newOpSetLit;
        },
        operands);
}

std::ostream& OpSetLit::dumpState(std::ostream& os) const {
    os << "OpSetLit: SetView=";
    prettyPrint(os, this->view());
    os << "\noperands=[";
    mpark::visit(
        [&](auto& operands) {
            bool first = true;
            for (const auto& operand : operands) {
                if (first) {
                    first = false;
                } else {
                    os << ",\n";
                }
                operand->dumpState(os);
            }
        },
        operands);
    os << "]";
    return os;
}

void OpSetLit::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    mpark::visit(
        [&](auto& operands) {
            for (auto& operand : operands) {
                operand = findAndReplace(operand, func);
            }
        },
        operands);
}
bool OpSetLit::isUndefined() { return this->numberUndefined > 0; }
pair<bool, ExprRef<SetView>> OpSetLit::optimise(PathExtension path) {
    auto returnExpr = mpark::get<ExprRef<SetView>>(path.expr);
    bool changeMade = false;
    mpark::visit(
        [&](auto& operands) {
            for (auto& operand : operands) {
                auto optResult = operand->optimise(path.extend(operand));
                changeMade |= optResult.first;
                operand = optResult.second;
            }
            typedef viewType(operands) View;
            ExprInterface<SetView>* commonSet = NULL;
            OpSetIndexInternal<View>* setIndexTest = NULL;
            bool first = true;
            for (auto& operand : operands) {
                setIndexTest =
                    dynamic_cast<OpSetIndexInternal<View>*>(&(*operand));
                if (!setIndexTest) {
                    commonSet = NULL;
                    break;
                }
                if (first) {
                    commonSet = &(*(setIndexTest->setOperand));
                } else if (commonSet != &(*(setIndexTest->setOperand))) {
                    commonSet = NULL;
                    break;
                }
            }
            if (commonSet) {
                returnExpr = setIndexTest->setOperand;
            }
        },
        this->operands);
    return make_pair(changeMade, returnExpr);
}
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSetLit> {
    static ExprRef<SetView> make(AnyExprVec operands);
};

ExprRef<SetView> OpMaker<OpSetLit>::make(AnyExprVec o) {
    return make_shared<OpSetLit>(move(o));
}
