#include "operators/opMinMax.h"
#include <algorithm>
#include <cassert>
#include "operators/flatten.h"
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "types/sequenceVal.h"
#include "utils/ignoreUnused.h"

using namespace std;

template <bool minMode>
using OperandsSequenceTrigger =
    typename OperatorTrates<OpMinMax<minMode>>::OperandsSequenceTrigger;

template <bool minMode>
void updateMinValues(OpMinMax<minMode>& op, bool trigger);

template <bool minMode>
bool OpMinMax<minMode>::compare(Int u, Int v) {
    return (minMode) ? u < v : v < u;
}

template <bool minMode>
void OpMinMax<minMode>::reevaluateImpl(SequenceView&) {
    minValueIndices.clear();
    this->setDefined(true);
    updateMinValues(*this, false);
}
template <bool minMode>
void triggerChange(OpMinMax<minMode>& op, bool wasDefined, Int oldValue) {
    if (!wasDefined && !op.isDefined()) {
        op.notifyValueDefined();
    } else if (wasDefined && !op.isDefined()) {
        op.notifyValueUndefined();
    } else if (op.isDefined()) {
        swap(op.value, oldValue);
        op.changeValue([&]() {
            swap(op.value, oldValue);
            return true;
        });
    }
}

template <bool minMode>
inline void updateMinValues(OpMinMax<minMode>& op, bool trigger) {
    bool wasDefined = op.isDefined();

    auto operandView = op.operand->getViewIfDefined();
    if (!operandView) {
        op.setDefined(false);
        triggerChange(op, wasDefined, op.value);
        return;
    }
    auto& members = (*operandView).template getMembers<IntView>();
    bool foundUndefined = false;
    Int oldValue = op.value;
    for (size_t i = 0; i < members.size(); ++i) {
        auto& operandChild = members[i];
        auto operandChildView = operandChild->getViewIfDefined();
        if (!operandChildView) {
            foundUndefined = true;
            continue;
        }
        Int operandValue = operandChildView->value;
        if (op.minValueIndices.empty() || op.compare(operandValue, op.value)) {
            op.value = operandValue;
            op.minValueIndices.clear();
            op.minValueIndices.insert(i);
        } else if (operandValue == op.value) {
            op.minValueIndices.insert(i);
        }
    }
    op.setDefined(!foundUndefined && !op.minValueIndices.empty());
    if (trigger) {
        triggerChange(op, wasDefined, oldValue);
    }
}

// returns true if the function did a full revaluate of the OpMinMax node
template <bool minMode>
inline bool handleOperandValueChange(OpMinMax<minMode>& op, Int index) {
    const ExprRef<IntView> expr =
        op.operand->view()->template getMembers<IntView>()[index];
    auto exprView = expr->view();
    if (!exprView) {
        op.handleMemberUndefined(index);
        return false;
    }
    bool fullReevaluate = false;
    if (op.compare((*exprView).value, op.value)) {
        op.value = (*exprView).value;
        op.minValueIndices.clear();
        op.minValueIndices.insert(index);
    } else if ((*exprView).value == op.value) {
        op.minValueIndices.insert(index);
    } else {
        // otherwise value is greater, needs to be removed
        op.minValueIndices.erase(index);
        if (op.minValueIndices.size() == 0) {
            // new min needs to be found
            fullReevaluate = true;
            op.reevaluate();
        }
    }
    return fullReevaluate;
}
template <bool minMode>
void OpMinMax<minMode>::handleMemberUndefined(UInt index) {
    this->setUndefinedAndTrigger();
    if (minValueIndices.count(index)) {
        minValueIndices.erase(index);
    }
    if (minValueIndices.empty()) {
        updateMinValues(*this, true);
    }
}
template <bool minMode>
void OpMinMax<minMode>::handleMemberDefined(UInt index) {
    auto& expr = this->operand->view()->template getMembers<IntView>()[index];
    auto exprView = expr->getViewIfDefined();
    if (!exprView) {
        return;
    }
    if (minValueIndices.empty() || compare((*exprView).value, this->value)) {
        minValueIndices.clear();
        minValueIndices.insert(index);
        this->value = (*exprView).value;
    } else if ((*exprView).value == this->value) {
        minValueIndices.insert(index);
    }
    this->setDefined(this->operand->appearsDefined());
}

template <bool minMode>
class OperatorTrates<OpMinMax<minMode>>::OperandsSequenceTrigger
    : public SequenceTrigger {
   public:
    OpMinMax<minMode>* op;
    OperandsSequenceTrigger(OpMinMax<minMode>* op) : op(op) {}
    void setNewValue(Int value) {
        if (!op->isDefined()) {
            op->value = value;
            op->setDefined(op->operand->appearsDefined());
            if (op->isDefined()) {
                op->notifyValueDefined();
            }
        } else {
            op->changeValue([&]() {
                op->value = value;
                return true;
            });
        }
    }

    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = lib::get<ExprRef<IntView>>(exprIn);
        auto view = expr->getViewIfDefined();

        // if is better
        if (view && (op->minValueIndices.empty() ||
                     op->compare((*view).value, op->value))) {
            op->minValueIndices.clear();
            op->minValueIndices.insert(index);
            setNewValue((*view).value);
            // if is equal
        } else if (view && (*view).value == op->value) {
            // need to add this index to set of min value indices.
            // must shift other indices up
            shiftIndicesUp(index, op->operand->view()->numberElements(),
                           op->minValueIndices);
            op->minValueIndices.insert(index);
            // if is worse
        } else if (!op->minValueIndices.empty()) {
            // it is ignored, still need to shift indices
            shiftIndicesUp(index, op->operand->view()->numberElements(),
                           op->minValueIndices);
            return;
        }
    }

    void valueRemoved(UInt index, const AnyExprRef&) final {
        if (op->minValueIndices.count(index)) {
            op->minValueIndices.erase(index);
        }
        if (!op->minValueIndices.empty()) {
            shiftIndicesDown(index, op->operand->view()->numberElements(),
                             op->minValueIndices);
        } else {
            updateMinValues(*op, true);
        }
    }

    inline void positionsSwapped(UInt index1, UInt index2) {
        if (op->minValueIndices.count(index1)) {
            if (!op->minValueIndices.count(index2)) {
                op->minValueIndices.erase(index1);
                op->minValueIndices.insert(index2);
            }
        } else {
            if (op->minValueIndices.count(index2)) {
                op->minValueIndices.erase(index2);
                op->minValueIndices.insert(index1);
            }
        }
    }
    void memberReplaced(UInt index, const AnyExprRef&) {
        subsequenceChanged(index, index + 1);
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        op->changeValue([&]() {
            for (size_t i = startIndex; i < endIndex; i++) {
                bool fullReevaluated = handleOperandValueChange(*op, i);
                if (fullReevaluated) {
                    break;
                }
            }
            return op->isDefined();
        });
    }

    void valueChanged() final {
        op->minValueIndices.clear();
        updateMinValues(*op, true);
    }
    void reattachTrigger() final {
        auto trigger = make_shared<
            OperatorTrates<OpMinMax<minMode>>::OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }
    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
    void memberHasBecomeUndefined(UInt index) final {
        op->handleMemberUndefined(index);
    }

    void memberHasBecomeDefined(UInt index) final {
        op->handleMemberDefined(index);
    }
};

template <bool minMode>
void OpMinMax<minMode>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    auto operandView = this->operand->getViewIfDefined();
    if (!operandView) {
        this->operand->updateVarViolations(vioContext, vioContainer);
        return;
    }
    for (auto& operandChild : (*operandView).template getMembers<IntView>()) {
        operandChild->updateVarViolations(vioContext, vioContainer);
    }
}

template <bool minMode>
void OpMinMax<minMode>::copy(OpMinMax<minMode>& newOp) const {
    newOp.value = this->value;
    newOp.minValueIndices = minValueIndices;
}
template <bool minMode>
std::ostream& OpMinMax<minMode>::dumpState(std::ostream& os) const {
    os << "OpMinMax: " << ((minMode) ? "minimising" : "maximising")
       << "value=" << this->value << endl;
    vector<Int> sortedOperands(minValueIndices.begin(), minValueIndices.end());
    sort(sortedOperands.begin(), sortedOperands.end());
    os << "Min ValuessIndices: " << sortedOperands << endl;
    return this->operand->dumpState(os);
}

template <bool minMode>
std::pair<bool, ExprRef<IntView>> OpMinMax<minMode>::optimiseImpl(
    ExprRef<IntView>& self, PathExtension path) {
    auto boolOpPair = this->standardOptimise(self, path);
    boolOpPair.first |= flatten<IntView>(*(boolOpPair.second));
    return boolOpPair;
}
template <bool minMode>
string OpMinMax<minMode>::getOpName() const {
    return toString("OpMinMax<minMode=", minMode, ">");
}

template <bool minMode>
void OpMinMax<minMode>::debugSanityCheckImpl() const {
    this->operand->debugSanityCheck();
    auto operandView = this->operand->view();
    if (!operandView) {
        sanityCheck(!this->appearsDefined(),
                    "operator must be undefined as operand is undefined.");
        return;
    }
    auto& members = (*operandView).template getMembers<IntView>();
    FastIterableIntSet checkMinValueIndices(0, 0);
    Int checkValue;
    for (size_t i = 0; i < members.size(); ++i) {
        auto& operandChild = members[i];
        auto operandChildView = operandChild->getViewIfDefined();
        if (!operandChildView) {
            continue;
        }
        Int operandValue = operandChildView->value;
        if (checkMinValueIndices.empty() || compare(operandValue, checkValue)) {
            checkValue = operandValue;
            checkMinValueIndices.clear();
            checkMinValueIndices.insert(i);
        } else if (operandValue == checkValue) {
            checkMinValueIndices.insert(i);
        }
    }
    sanityEqualsCheck(checkMinValueIndices.size(), minValueIndices.size());
    for (const auto& index : checkMinValueIndices) {
        sanityCheck(
            minValueIndices.count(index),
            toString("index ", index, " is missing from minValueIndices."));
    }
    if (!checkMinValueIndices.empty()) {
        sanityEqualsCheck(checkValue, this->value);
        sanityCheck(this->appearsDefined(), "operator must be defined.");
    } else {
        sanityCheck(!this->appearsDefined(),
                    "empty min/max means this operator should be undefined.");
    }
}

template <typename Op>
struct OpMaker;

template <bool minMode>
struct OpMaker<OpMinMax<minMode>> {
    static ExprRef<IntView> make(
        ExprRef<SequenceView>,
        const shared_ptr<SequenceDomain>& operandDomain = nullptr);
};
template <typename View>
struct OpUndefined;
template <typename View>
struct OpMaker<OpUndefined<View>> {
    static ExprRef<View> make();
};

template <bool minMode>
ExprRef<IntView> OpMaker<OpMinMax<minMode>>::make(
    ExprRef<SequenceView> o, const shared_ptr<SequenceDomain>& operandDomain) {
    if (operandDomain &&
        lib::get_if<shared_ptr<EmptyDomain>>(&operandDomain->inner)) {
        // construct empty sequence of type int
        return OpMaker<OpUndefined<IntView>>::make();
    }
    return make_shared<OpMinMax<minMode>>(move(o));
}

template struct OpMinMax<true>;
template struct OpMaker<OpMinMax<true>>;
template struct OpMinMax<false>;
template struct OpMaker<OpMinMax<false>>;
;
