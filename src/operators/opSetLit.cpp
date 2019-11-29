#include "operators/opSetLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/opSetIndexInternal.h"
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;

ostream& operator<<(std::ostream& os, const OpSetLit::OperandGroup& og) {
    return os << "og(focus=" << og.focusOperand
              << ",setIndex=" << og.focusOperandSetIndex
              << ",operands=" << og.operands << ")" << endl;
}

template <typename View>
void OpSetLit::addOperandValue(size_t index, bool insert) {
    auto& expr = getOperands<View>()[index];
    HashType hash =
        getValueHash(expr->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));
    auto& og = hashIndicesMap[hash];
    debug_code(assert(!og.operands.count(index)));
    og.operands.insert(index);
    if (insert) {
        cachedOperandHashes.insert(index, hash);
    } else {
        cachedOperandHashes.set(index, hash);
    }
    if (!og.active || og.operands.size() == 1) {
        og.active = true;
        og.focusOperand = index;
        addMemberAndNotify(expr);
        og.focusOperandSetIndex = numberElements() - 1;
    }
}

template <typename View>
void OpSetLit::removeMemberFromSet(const HashType& hash,
                                   OpSetLit::OperandGroup& operandGroup) {
    UInt setIndex = operandGroup.focusOperandSetIndex;

    removeMemberAndNotify<View>(setIndex);
    if (setIndex < numberElements()) {
        // by deleting an element from the set, another member was moved.
        // Must tell that operand that its location in the set has changed
        hashIndicesMap[indexHashMap[setIndex]].focusOperandSetIndex = setIndex;
    }
}

template <typename View>
void OpSetLit::addReplacementToSet(OpSetLit::OperandGroup& og) {
    // search for an operand who's value has not changed.
    // It shall be made the replacement operand for this set.
    // If no operands have the same value, disable this operand group
    // temperarily.
    auto unchangedOperand =
        find_if(begin(og.operands), end(og.operands),
                [&](auto& o) { return hashNotChanged<View>(o); });
    if (unchangedOperand != og.operands.end()) {
        og.focusOperand = *unchangedOperand;
        addMemberAndNotify(getOperands<View>()[og.focusOperand]);
        og.focusOperandSetIndex = numberElements() - 1;
    } else {
        og.active = false;
    }
}

template <typename View>
void OpSetLit::removeOperandValue(size_t index, HashType hash) {
    auto iter = hashIndicesMap.find(hash);
    debug_code(assert(iter != hashIndicesMap.end()));
    auto& operandGroup = iter->second;
    debug_code(assert(operandGroup.operands.count(index)));
    operandGroup.operands.erase(index);
    if (operandGroup.active && operandGroup.focusOperand == index) {
        removeMemberFromSet<View>(hash, operandGroup);
        // If another operand had the same value, it can be used in place of
        // the one just deleted.  The problem is that each of the operands'
        // values might have changed.  So we filter for operands that have not
        // changed.  if no operands remain the same value, we temperarily
        // disable this operand group.
        addReplacementToSet<View>(operandGroup);
    }

    if (operandGroup.operands.empty()) {
        hashIndicesMap.erase(iter);
    }
}

template <typename View>
bool OpSetLit::hashNotChanged(UInt index) {
    debug_code(assert(index < getOperands<View>().size()));
    HashType operandHash =
        getValueHash(getOperands<View>()[index]->getViewIfDefined().checkedGet(
            NO_SET_UNDEFINED_MEMBERS));
    return cachedOperandHashes.get(index) == operandHash;
}
void OpSetLit::evaluateImpl() {
    debug_code(assert(exprTriggers.empty()));
    lib::visit(
        [&](auto& operands) {
            this->members.emplace<ExprRefVec<viewType(operands)>>();
            for (size_t i = 0; i < operands.size(); i++) {
                auto& operand = operands[i];
                operand->evaluate();
                if (!operand->appearsDefined()) {
                    myCerr << NO_SET_UNDEFINED_MEMBERS;
                    myAbort();
                }
                addOperandValue<viewType(operands)>(i, true);
            }
        },
        operands);
    this->setAppearsDefined(true);
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>,
      public OpSetLit::ExprTriggerBase {
    typedef typename AssociatedViewType<TriggerType>::type View;

    using ExprTriggerBase::index;
    using ExprTriggerBase::op;

    ExprTrigger(OpSetLit* op, UInt index)
        : ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>(
              lib::get<ExprRefVec<View>>(op->operands)[index]),
          ExprTriggerBase(op, index) {}
    ExprRef<View>& getTriggeringOperand() {
        return lib::get<ExprRefVec<View>>(op->operands)[this->index];
    }
    void adapterValueChanged() {
        op->removeOperandValue<View>(index, op->cachedOperandHashes.get(index));
        op->addOperandValue<View>(index);
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

void OpSetLit::startTriggeringImpl() {
    if (exprTriggers.empty()) {
        lib::visit(
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
        lib::visit(
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
        lib::visit(
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
    lib::visit(
        [&](auto& operands) {
            for (auto& operand : operands) {
                operand->updateVarViolations(context, container);
            }
        },
        operands);
}

ExprRef<SetView> OpSetLit::deepCopyForUnrollImpl(
    const ExprRef<SetView>&, const AnyIterRef& iterator) const {
    return lib::visit(
        [&](auto& operands) {
            ExprRefVec<viewType(operands)> newOperands;
            ExprRefVec<viewType(operands)> newSetMembers(numberElements(),
                                                         nullptr);

            for (auto& operand : operands) {
                newOperands.emplace_back(
                    operand->deepCopyForUnroll(operand, iterator));
            }
            auto newOpSetLit = make_shared<OpSetLit>(move(newOperands));
            return newOpSetLit;
        },
        operands);
}

std::ostream& OpSetLit::dumpState(std::ostream& os) const {
    os << "OpSetLit: SetView=";
    prettyPrint(os, this->view());
    os << "\noperands=[";
    lib::visit(
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

void OpSetLit::findAndReplaceSelf(const FindAndReplaceFunction& func,
                                  PathExtension path) {
    lib::visit(
        [&](auto& operands) {
            for (auto& operand : operands) {
                operand = findAndReplace(operand, func, path);
            }
        },
        operands);
}

pair<bool, ExprRef<SetView>> OpSetLit::optimiseImpl(ExprRef<SetView>&,
                                                    PathExtension path) {
    auto newOp = std::make_shared<OpSetLit>(operands);
    AnyExprRef newOpAsExpr = ExprRef<SetView>(newOp);
    bool optimised = false;
    lib::visit(
        [&](auto& operands) {
            for (auto& operand : operands) {
                optimised |= optimise(newOpAsExpr, operand, path);
            }
        },
        newOp->operands);

    return std::make_pair(optimised, newOp);
}

string OpSetLit::getOpName() const { return "OpSetLit"; }

void OpSetLit::debugSanityCheckImpl() const {
    lib::visit(
        [&](auto& operands) {
            for (auto& o : operands) {
                o->debugSanityCheck();
            }
            standardSanityChecksForThisType();

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
                sanityCheck(og.active,
                            "There should not be any inactive operands.");
                sanityCheck(og.operands.count(og.focusOperand),
                            "focus operand not in operand group.");
                sanityEqualsCheck(hash,
                                  indexHashMap.at(og.focusOperandSetIndex));
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

AnyExprVec& OpSetLit::getChildrenOperands() { return operands; }
