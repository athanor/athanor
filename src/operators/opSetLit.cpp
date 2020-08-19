#include "operators/opSetLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/opSetIndexInternal.h"
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "types/function.h"
#include "utils/ignoreUnused.h"
using namespace std;
template <typename OperandView>
ostream& operator<<(std::ostream& os,
                    const typename OpSetLit<OperandView>::OperandGroup& og) {
    return os << "og(focus=" << og.focusOperand
              << ",setIndex=" << og.focusOperandSetIndex
              << ",operands=" << og.operands << ")" << endl;
}

template <typename OperandView>
template <typename InnerViewType>
void OpSetLit<OperandView>::addOperandValue(ExprRefVec<InnerViewType>& operands,
                                            size_t index, bool insert) {
    auto& expr = operands[index];
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
        this->addMemberAndNotify(expr);
        og.focusOperandSetIndex = this->numberElements() - 1;
    }
}

template <typename OperandView>
template <typename InnerViewType>
void OpSetLit<OperandView>::removeMemberFromSet(
    const HashType&, OpSetLit<OperandView>::OperandGroup& operandGroup) {
    UInt setIndex = operandGroup.focusOperandSetIndex;

    this->template removeMemberAndNotify<InnerViewType>(setIndex);

    if (setIndex < this->numberElements()) {
        // by deleting an element from the set, another member was moved.
        // Must tell that operand that its location in the set has changed
        hashIndicesMap[this->indexHashMap[setIndex]].focusOperandSetIndex =
            setIndex;
    }
}

template <typename OperandView>
template <typename InnerViewType>
void OpSetLit<OperandView>::addReplacementToSet(
    ExprRefVec<InnerViewType>& operands,
    OpSetLit<OperandView>::OperandGroup& og) {
    // search for an operand who's value has not changed.
    // It shall be made the replacement operand for this set.
    // If no operands have the same value, disable this operand group
    // temperarily.
    auto unchangedOperand =
        find_if(begin(og.operands), end(og.operands), [&](auto& o) {
            return this->template hashNotChanged<InnerViewType>(operands, o);
        });
    if (unchangedOperand != og.operands.end()) {
        og.focusOperand = *unchangedOperand;
        this->addMemberAndNotify(operands[og.focusOperand]);
        og.focusOperandSetIndex = this->numberElements() - 1;
    } else {
        og.active = false;
    }
}

template <typename OperandView>
template <typename InnerViewType>
void OpSetLit<OperandView>::removeOperandValue(
    ExprRefVec<InnerViewType>& operands, size_t index, HashType hash) {
    auto iter = hashIndicesMap.find(hash);
    debug_code(assert(iter != hashIndicesMap.end()));
    auto& operandGroup = iter->second;
    debug_code(assert(operandGroup.operands.count(index)));
    operandGroup.operands.erase(index);
    if (operandGroup.active && operandGroup.focusOperand == index) {
        removeMemberFromSet<InnerViewType>(hash, operandGroup);
        // If another operand had the same value, it can be used in place of
        // the one just deleted.  The problem is that each of the operands'
        // values might have changed.  So we filter for operands that have not
        // changed.  if no operands remain the same value, we temperarily
        // disable this operand group.
        addReplacementToSet(operands, operandGroup);
    }

    if (operandGroup.operands.empty()) {
        hashIndicesMap.erase(iter);
    }
}

template <typename OperandView>
template <typename InnerViewType>
bool OpSetLit<OperandView>::hashNotChanged(ExprRefVec<InnerViewType>& operands,
                                           UInt index) {
    debug_code(assert(index < operands.size()));
    HashType operandHash =
        getValueHash(operands[index]->getViewIfDefined().checkedGet(
            NO_SET_UNDEFINED_MEMBERS));
    return cachedOperandHashes.get(index) == operandHash;
}

AnyExprVec& getMembersFromOperand(FunctionView& function) {
    return function.range;
}

template <typename OperandView>
void OpSetLit<OperandView>::reevaluateImpl(OperandView& operand) {
    auto& operands = getMembersFromOperand(operand);
    hashIndicesMap.clear();
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

    void imageChanged(UInt index) final {
        debug_log("hit\n");
        auto& view = op->operand->view().checkedGet(NO_SET_UNDEFINED_MEMBERS);
        mpark::visit(
            [&](auto& operands) {
                op->removeOperandValue(operands, index,
                                       op->cachedOperandHashes.get(index));
                op->addOperandValue(operands, index);
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

            for (const auto& hashIndicesPair : this->hashIndicesMap) {
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
template <typename OperandView>
AnyExprVec& OpSetLit<OperandView>::getChildrenOperands() {
    return getMembersFromOperand(this->operand->view().checkedGet(
        "Could not get operands for OpSetLit"));
}

template struct OpSetLit<FunctionView>;
