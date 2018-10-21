#include "operators/opSetLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/opSetIndexInternal.h"
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;
static const char* ERROR_NO_UNDEFINED_MEMBERS =
    "Error: not handling OpSetLit with undefined values at the moment.\n";

template <typename View>
void OpSetLit::addValue(size_t index, bool insert) {
    auto& expr = getOperands<View>()[index];
    HashType hash =
        getValueHash(expr->view().checkedGet(ERROR_NO_UNDEFINED_MEMBERS));
    auto& operandGroup = hashIndicesMap[hash];
    debug_code(assert(!operandGroup.operands.count(index)));
    operandGroup.operands.insert(index);
    if (insert) {
        cachedOperandHashes.insert(index, hash);
    } else {
        cachedOperandHashes.set(index, hash);
    }
    if (operandGroup.operands.size() == 1) {
        operandGroup.focusOperand = index;
        addMemberAndNotify(expr);
        operandGroup.focusOperandSetIndex = numberElements() - 1;
        cachedSetHashes.insert(numberElements() - 1, hash);
    }
}

template <typename View>
void OpSetLit::removeValue(size_t index, HashType hash) {
    auto iter = hashIndicesMap.find(hash);
    debug_code(assert(iter != hashIndicesMap.end()));
    auto& operandGroup = iter->second;
    debug_code(assert(operandGroup.operands.count(index)));
    operandGroup.operands.erase(index);
    if (operandGroup.focusOperand == index) {
        UInt setIndex = operandGroup.focusOperandSetIndex;
        removeMemberAndNotify<View>(setIndex);
        cachedSetHashes.swapErase(setIndex);
        if (setIndex < numberElements()) {
            hashIndicesMap[cachedSetHashes.get(setIndex)].focusOperandSetIndex =
                setIndex;
        }
        if (operandGroup.operands.empty()) {
            hashIndicesMap.erase(iter);
        } else {
            operandGroup.focusOperand = *(operandGroup.operands.begin());
            addMemberAndNotify(getOperands<View>()[operandGroup.focusOperand]);
            operandGroup.focusOperandSetIndex = numberElements() - 1;
        }
    }
}

void OpSetLit::evaluateImpl() {
    debug_code(assert(exprTriggers.empty()));
    mpark::visit(
        [&](auto& operands) {
            this->members.emplace<ExprRefVec<viewType(operands)>>();
            for (size_t i = 0; i < operands.size(); i++) {
                auto& operand = operands[i];
                operand->evaluate();
                if (!operand->appearsDefined()) {
                    cerr << ERROR_NO_UNDEFINED_MEMBERS;
                    abort();
                }
                addValue<viewType(operands)>(i, true);
            }

        },
        operands);
    this->setAppearsDefined(true);
    debug_code(assertValidHashes());
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public OpSetLit::ExprTriggerBase,
      public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type View;
    using ExprTriggerBase::ExprTriggerBase;
    using ExprTriggerBase::index;
    using ExprTriggerBase::op;
    ExprRef<View>& getTriggeringOperand() {
        return mpark::get<ExprRefVec<View>>(op->operands)[this->index];
    }
    void adapterValueChanged() {
        op->removeValue<View>(index, op->cachedOperandHashes.get(index));
        op->addValue<View>(index);
    }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
            op->exprTriggers[index]));
        auto trigger = make_shared<ExprTrigger<TriggerType>>(op, index);
        op->getMembers<View>()[index]->addTrigger(trigger);
        op->exprTriggers[index] = trigger;
    }

    void adapterHasBecomeDefined() { todoImpl(); }
    void adapterHasBecomeUndefined() { todoImpl(); }
};
}  // namespace

OpSetLit::OpSetLit(OpSetLit&& other)
    : SetView(std::move(other)),
      operands(move(other.operands)),
      hashIndicesMap(move(other.hashIndicesMap)),
      exprTriggers(move(other.exprTriggers)) {
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

ostream& operator<<(ostream& os, const OpSetLit::OperandGroup& og) {
    os << "OperandGroup (";
    os << "focusOperand=" << og.focusOperand << ",\n";
    os << "focusOperandSetIndex=" << og.focusOperandSetIndex << ",\n";
    os << "operands=" << og.operands << ")\n";
    return os;
}

void OpSetLit::assertValidHashes() {
    bool success = true;
    mpark::visit(
        [&](auto& operands) {
            for (size_t i = 0; i < operands.size(); i++) {
                auto& operand = operands[i];
                if (!operand->appearsDefined()) {
                    cerr << NO_SET_UNDEFINED_MEMBERS;
                    success = false;
                    break;
                    continue;
                }
                HashType hash = getValueHash(
                    operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
                if (cachedOperandHashes.get(i) != hash) {
                    cerr << "Error: cachedOperandHashes at index " << i
                         << " has value " << cachedOperandHashes.get(i)
                         << " but operand has value " << hash << endl;
                    success = false;
                    break;
                }
                auto iter = hashIndicesMap.find(hash);
                if (iter == hashIndicesMap.end()) {
                    success = false;
                    cerr << "Error, hash " << hash << " of operand ";
                    operand->dumpState(cerr);
                    cerr << " maps to no operand group.\n";
                    break;
                }
                auto& og = iter->second;
                if (!og.operands.count(i)) {
                    success = false;
                    operand->dumpState(cerr << "error: operand: ")
                        << " with hash " << hash
                        << " maps to an operand group not containing the "
                           "operand "
                           "index "
                        << i << endl;
                    cerr << og << endl;
                    break;
                }
            }

            if (success) {
                for (const auto& hashIndicesPair : hashIndicesMap) {
                    HashType hash = hashIndicesPair.first;
                    auto& og = hashIndicesPair.second;
                    if (!og.operands.count(og.focusOperand)) {
                        cerr << "Error: focus operand not in operand group: "
                             << og << endl;
                        success = false;
                        break;
                    }
                    if (cachedSetHashes.get(og.focusOperandSetIndex) != hash) {
                        cerr << "Error: cached set indices does not match up "
                                "with operand group "
                             << og << endl;
                        success = false;
                        break;
                    }
                    for (UInt index : og.operands) {
                        HashType hash2 = cachedOperandHashes.get(index);
                        if (hash2 != hash) {
                            cerr << "Error: index " << index << " maps to hash "
                                 << hash2 << " but hash " << hash << " maps to "
                                 << index << endl;
                            success = false;
                            break;
                        }
                    }
                }
            }
            if (!success) {
                cerr << "HashIndicesMap: " << hashIndicesMap << endl;
                cerr << "cachedOperandHashes: " << cachedOperandHashes.contents
                     << endl;
                cerr << "cachedSetHashes: " << cachedSetHashes.contents << endl;
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

string OpSetLit::getOpName() const { return "OpSetLit"; }

void OpSetLit::debugSanityCheckImpl() const {
    mpark::visit(
        [&](auto& operands) {
            for (size_t i = 0; i < operands.size(); i++) {
                auto& operand = operands[i];
                auto view = operand->getViewIfDefined();
                sanityCheck(view, NO_SET_UNDEFINED_MEMBERS);
                HashType hash = getValueHash(*view);
                sanityEqualsCheck(hash, cachedOperandHashes.get(i));
                auto iter = hashIndicesMap.find(hash);
                sanityCheck(iter != hashIndicesMap.end(),
                            toString("hash ", hash, " of operand with index ",
                                     i, " maps to no operand group."));
                auto& og = iter->second;
                sanityCheck(og.operands.count(i),
                            toString("hash ", hash,
                                     " maps to operand group that does not "
                                     "contain the index ",
                                     i));
            }

            for (const auto& hashIndicesPair : hashIndicesMap) {
                HashType hash = hashIndicesPair.first;
                auto& og = hashIndicesPair.second;
                sanityCheck(og.operands.count(og.focusOperand),
                            "focus operand not in operand group.");
                sanityEqualsCheck(hash,
                                  cachedSetHashes.get(og.focusOperandSetIndex));
                for (UInt index : og.operands) {
                    HashType hash2 = cachedOperandHashes.get(index);
                    sanityEqualsCheck(hash, hash2);
                }
            }
        },
        operands);
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
