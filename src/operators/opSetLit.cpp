#include "operators/opSetLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/opSetIndexInternal.h"
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "types/function.h"
#include "types/sequence.h"
#include "utils/ignoreUnused.h"
using namespace std;
template <typename OperandView>
template <typename InnerViewType>
void OpSetLit<OperandView>::addOperandValue(ExprRefVec<InnerViewType>& operands,
                                            size_t index, bool insert) {
    auto& expr = operands[index];
    HashType hash =
        getValueHash(expr->view().checkedGet(NO_SET_UNDEFINED_MEMBERS));

    if (insert) {
        if (index < cachedOperandHashes.size()) {
            shiftIndicesUp(index);
        }
        cachedOperandHashes.insert(index, hash);
    } else {
        cachedOperandHashes.set(index, hash);
    }

    auto& og = hashOperandsMap[hash];
    debug_code(assert(!og.operands.count(index)));
    og.operands.insert(index);
    if (!og.active || og.operands.size() == 1) {
        og.active = true;
        og.focusOperand = index;
        this->addMemberAndNotify(expr);
        og.focusOperandSetIndex = this->numberElements() - 1;
    }
}

template <typename OperandView>

template <typename InnerViewType>
void OpSetLit<OperandView>::addReplacementToSet(
    ExprRefVec<InnerViewType>& operands,
    OpSetLit<OperandView>::OperandGroup& og,
    lib::optional<UInt> toBeDeletedIndex) {
    // search for an operand who's value has not changed.
    // It shall be made the replacement operand for this set.
    // If no operands have the same value, disable this operand group
    // temperarily.
    auto hashNotChanged = [&](UInt operandHashIndex) {
        auto operandIndex =
            (toBeDeletedIndex && operandHashIndex > *toBeDeletedIndex)
                ? operandHashIndex - 1
                : operandHashIndex;
        // operandHashIndex is the index of where our cached hash of the operand
        // is. operandIndex is usually the same as operandHashIndex, but if an
        // operand was deleted, everything after will be shifted down by one,
        // because the operand was dleted but we have not updated the caches
        // array yet.
        return operandIndex < operands.size() &&
               cachedOperandHashes.get(operandHashIndex) ==
                   getValueHash(operands[operandIndex]);
    };

    auto unchangedOperand =
        find_if(begin(og.operands), end(og.operands), hashNotChanged);
    if (unchangedOperand != og.operands.end()) {
        og.focusOperand = *unchangedOperand;
        bool shifted = toBeDeletedIndex && og.focusOperand > *toBeDeletedIndex;
        this->addMemberAndNotify(operands[og.focusOperand - int(shifted)]);
        og.focusOperandSetIndex = this->numberElements() - 1;
    } else {
        og.active = false;
    }
}

template <typename OperandView>
template <typename InnerViewType>
void OpSetLit<OperandView>::removeOperandValue(
    ExprRefVec<InnerViewType>& operands, size_t index, bool shouldDelete) {
    debug_code(assert(index < cachedOperandHashes.size()));
    HashType hash = this->cachedOperandHashes.get(index);
    auto iter = hashOperandsMap.find(hash);
    debug_code(assert(iter != hashOperandsMap.end()));
    auto& operandGroup = iter->second;
    debug_code(assert(operandGroup.operands.count(index)));
    operandGroup.operands.erase(index);
    if (operandGroup.active && operandGroup.focusOperand == index) {
        auto setIndex = operandGroup.focusOperandSetIndex;

        this->template removeMemberAndNotify<InnerViewType>(setIndex);
        if (setIndex < this->numberElements()) {
            // by deleting an element from the set, another member was moved.
            // Must tell that operand that its location in the set has changed
            hashOperandsMap.at(this->indexHashMap.at(setIndex))
                .focusOperandSetIndex = setIndex;
        }

        // If another operand had the same value, it can be used in place of
        // the one just deleted.  The problem is that each of the operands'
        // values might have changed.  So we filter for operands that have not
        // changed.  if no operands remain the same value, we temperarily
        // disable this operand group.
        lib::optional<UInt> toBeDeletedIndex;
        if (shouldDelete) {
            toBeDeletedIndex = index;
        }
        addReplacementToSet(operands, operandGroup, toBeDeletedIndex);
    }
    if (operandGroup.operands.empty()) {
        hashOperandsMap.erase(iter);
    }
    if (shouldDelete) {
        cachedOperandHashes.erase(index);
        if (index < cachedOperandHashes.size()) {
            shiftIndicesDown(index);
        }
    }
}

template <typename OperandView>
void OpSetLit<OperandView>::shiftIndicesDown(UInt index) {
    vector<UInt> buffer;
    for (auto& mapping : this->hashOperandsMap) {
        auto& og = mapping.second;
        if (og.focusOperand > index) {
            og.focusOperand -= 1;
        }
        for (auto operandIndex : og.operands) {
            if (operandIndex > index) {
                buffer.emplace_back(operandIndex);
            }
        }
        for (auto operandIndex : buffer) {
            og.operands.erase(operandIndex);
        }
        for (auto operandIndex : buffer) {
            og.operands.emplace(operandIndex - 1);
        }
        buffer.clear();
    }
}

template <typename OperandView>
void OpSetLit<OperandView>::shiftIndicesUp(UInt index) {
    vector<UInt> buffer;
    for (auto& mapping : this->hashOperandsMap) {
        auto& og = mapping.second;
        if (og.focusOperand >= index) {
            og.focusOperand += 1;
        }
        for (auto operandIndex : og.operands) {
            if (operandIndex >= index) {
                buffer.emplace_back(operandIndex);
            }
        }
        for (auto operandIndex : buffer) {
            og.operands.erase(operandIndex);
        }
        for (auto operandIndex : buffer) {
            og.operands.emplace(operandIndex + 1);
        }
        buffer.clear();
    }
}
AnyExprVec& getMembersFromOperand(FunctionView& function) {
    return function.range;
}

AnyExprVec& getMembersFromOperand(SequenceView& sequence) {
    return sequence.members;
}

template <typename OperandView>
void OpSetLit<OperandView>::reevaluateImpl(OperandView& operand) {
    auto& operands = getMembersFromOperand(operand);
    hashOperandsMap.clear();
    cachedOperandHashes.clear();

    lib::visit(
        [&](auto& operands) {
            this->members.template emplace<ExprRefVec<viewType(operands)>>();
            for (size_t i = 0; i < operands.size(); i++) {
                auto& operand = operands[i];
                operand->evaluate();
                if (!operand->appearsDefined()) {
                    myCerr << NO_SET_UNDEFINED_MEMBERS;
                    myAbort();
                }
                addOperandValue<viewType(operands)>(operands, i, true);
            }
        },
        operands);
    this->setAppearsDefined(true);
}

struct OperatorTrates<OpSetLit<FunctionView>>::OperandTrigger
    : public FunctionTrigger {
    OpSetLit<FunctionView>* op;

    OperandTrigger(OpSetLit<FunctionView>* op) : op(op) {}
    void valueChanged() final {
        op->reevaluate();
        op->notifyEntireValueChanged();
    }
    void hasBecomeUndefined() final { todoImpl(); }
    void hasBecomeDefined() final { todoImpl(); }
    void memberHasBecomeUndefined(UInt) final { todoImpl(); }
    void memberHasBecomeDefined(UInt) final { todoImpl(); }
    void preimageChanged(UInt, HashType) final {}
    void preimageChanged(const std::vector<UInt>&,
                         const std::vector<HashType>&) {}
    void imageChanged(UInt index) final {
        auto& view = op->operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS);
        mpark::visit(
            [&](auto& operands) {
                op->removeOperandValue(operands, index, false);
                op->addOperandValue(operands, index, false);
            },
            getMembersFromOperand(view));
    }
    void imageChanged(const std::vector<UInt>& indices) {
        for (auto index : indices) {
            this->imageChanged(index);
        }
    }
    void memberReplaced(UInt index, const AnyExprRef&) final {
        imageChanged(index);
    }
    void valueRemoved(UInt index, const AnyExprRef&, const AnyExprRef&) {
        auto& view = op->operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS);
        mpark::visit(
            [&](auto& operands) {
                op->removeOperandValue(
                    operands, op->cachedOperandHashes.size() - 1, true);
                if (index == op->cachedOperandHashes.size()) {
                    return;
                }
                op->removeOperandValue(operands, index, false);
                op->addOperandValue(operands, index, false);
            },
            getMembersFromOperand(view));
    }
    void valueAdded(const AnyExprRef&, const AnyExprRef&) {
        auto& view = op->operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS);
        mpark::visit(
            [&](auto& operands) {
                op->addOperandValue(operands, op->cachedOperandHashes.size(),
                                    true);
            },
            getMembersFromOperand(view));
    }
    void imageSwap(UInt index1, UInt index2) {
        imageChanged(index1);
        imageChanged(index2);
    }

    void reattachTrigger() final {
        auto trigger =
            make_shared<OperatorTrates<OpSetLit<FunctionView>>::OperandTrigger>(
                op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }
};

struct OperatorTrates<OpSetLit<SequenceView>>::OperandTrigger
    : public SequenceTrigger {
    OpSetLit<SequenceView>* op;

    OperandTrigger(OpSetLit<SequenceView>* op) : op(op) {}
    void valueChanged() final {
        op->reevaluate();
        op->notifyEntireValueChanged();
    }
    void hasBecomeUndefined() final { todoImpl(); }
    void hasBecomeDefined() final { todoImpl(); }
    void memberHasBecomeUndefined(UInt) final { todoImpl(); }
    void memberHasBecomeDefined(UInt) final { todoImpl(); }
    void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        auto& view = op->operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS);
        mpark::visit(
            [&](auto& operands) {
                for (size_t index = startIndex; index < endIndex; index++) {
                    op->removeOperandValue(operands, index, false);
                    op->addOperandValue(operands, index, false);
                }
            },
            getMembersFromOperand(view));
    }

    void memberReplaced(UInt index, const AnyExprRef&) final {
        subsequenceChanged(index, index + 1);
    }
    void valueRemoved(UInt index, const AnyExprRef&) {
        auto& view = op->operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS);
        mpark::visit(
            [&](auto& operands) {
                op->removeOperandValue(operands, index, true);
            },
            getMembersFromOperand(view));
    }
    void valueAdded(UInt index, const AnyExprRef&) {
        auto& view = op->operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS);
        mpark::visit(
            [&](auto& operands) { op->addOperandValue(operands, index, true); },
            getMembersFromOperand(view));
    }
    void positionsSwapped(UInt index1, UInt index2) {
        subsequenceChanged(index1, index1 + 1);
        subsequenceChanged(index2, index2 + 1);
    }

    void reattachTrigger() final {
        auto trigger =
            make_shared<OperatorTrates<OpSetLit<SequenceView>>::OperandTrigger>(
                op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }
};

template <typename OperandView>
void OpSetLit<OperandView>::updateVarViolationsImpl(
    const ViolationContext& context, ViolationContainer& container) {
    this->operand->updateVarViolations(context, container);
}

template <typename OperandView>
void OpSetLit<OperandView>::copy(OpSetLit<OperandView>&) const {}

template <typename OperandView>
std::ostream& OpSetLit<OperandView>::dumpState(std::ostream& os) const {
    os << "OpSetLit<OperandView>: SetView=";
    prettyPrint(os, this->view());
    os << ",\ncachedOperandHashes=" << cachedOperandHashes
       << ",\nhashOperandsMap=" << hashOperandsMap;
    os << "\noperand=[";
    return this->operand->dumpState(os) << endl;
}

template <typename OperandView>
string OpSetLit<OperandView>::getOpName() const {
    return toString(
        "OpSetLit<",
        TypeAsString<typename AssociatedValueType<OperandView>::type>::value,
        ">");
}

template <typename OperandView>
void OpSetLit<OperandView>::debugSanityCheckImpl() const {
    this->operand->debugSanityCheck();
    this->standardSanityChecksForThisType();
    sanityCheck(this->operand->getViewIfDefined().hasValue(),
                "should be defined.");
    lib::visit(
        [&](auto& operands) {
            for (size_t i = 0; i < operands.size(); i++) {
                auto& operand = operands[i];
                auto view = operand->getViewIfDefined();
                sanityCheck(view, NO_SET_UNDEFINED_MEMBERS);
                HashType hash = getValueHash(*view);
                sanityEqualsCheck(hash, cachedOperandHashes.get(i));
                auto iter = hashOperandsMap.find(hash);
                sanityCheck(iter != hashOperandsMap.end(),
                            toString("hash ", hash, " of operand with index ",
                                     i, " maps to no operand group."));
                auto& og = iter->second;
                sanityCheck(og.operands.count(i),
                            toString("hash ", hash,
                                     " maps to operand group that does not "
                                     "contain the index ",
                                     i));
            }

            for (const auto& hashIndicesPair : this->hashOperandsMap) {
                HashType hash = hashIndicesPair.first;
                auto& og = hashIndicesPair.second;
                sanityCheck(og.active,
                            "There should not be any inactive operands.");
                sanityCheck(og.operands.count(og.focusOperand),
                            "focus operand not in operand group.");
                sanityEqualsCheck(
                    hash, this->indexHashMap.at(og.focusOperandSetIndex));
                for (UInt index : og.operands) {
                    HashType hash2 = cachedOperandHashes.get(index);
                    sanityEqualsCheck(hash, hash2);
                }
            }
        },
        getMembersFromOperand(this->operand->getViewIfDefined().get()));
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSetLit<FunctionView>> {
    static ExprRef<SetView> make(ExprRef<FunctionView> o);
};

ExprRef<SetView> OpMaker<OpSetLit<FunctionView>>::make(
    ExprRef<FunctionView> o) {
    return make_shared<OpSetLit<FunctionView>>(move(o));
}

template <>
struct OpMaker<OpSetLit<SequenceView>> {
    static ExprRef<SetView> make(ExprRef<SequenceView> o);
};

ExprRef<SetView> OpMaker<OpSetLit<SequenceView>>::make(
    ExprRef<SequenceView> o) {
    return make_shared<OpSetLit<SequenceView>>(move(o));
}

template <typename OperandView>
AnyExprVec& OpSetLit<OperandView>::getChildrenOperands() {
    return getMembersFromOperand(this->operand->view().checkedGet(
        "Could not get operands for OpSetLit"));
}

template struct OpSetLit<FunctionView>;
template struct OpSetLit<SequenceView>;
