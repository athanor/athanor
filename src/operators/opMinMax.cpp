
#include "operators/opMinMax.h"
#include <algorithm>
#include <cassert>
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
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
void OpMinMax<minMode>::reevaluate() {
    minValueIndices.clear();
    this->setDefined(!this->operand->isUndefined(), false);
    updateMinValues(*this, false);
}

template <bool minMode>
inline void updateMinValues(OpMinMax<minMode>& op, bool trigger) {
    auto& members = op.operand->view().template getMembers<IntView>();
    bool foundUndefined = false;
    Int oldValue = op.value;
    bool wasDefined = op.isDefined();
    for (size_t i = 0; i < members.size(); ++i) {
        auto& operandChild = members[i];
        if (operandChild->isUndefined()) {
            foundUndefined = true;
            continue;
        }
        Int operandValue = operandChild->view().value;
        if (op.minValueIndices.empty() || op.compare(operandValue, op.value)) {
            op.value = operandValue;
            op.minValueIndices.clear();
            op.minValueIndices.insert(i);
        } else if (operandValue == op.value) {
            op.minValueIndices.insert(i);
        }
    }
    if (op.minValueIndices.empty()) {
        op.setDefined(false, trigger);
    } else if (!foundUndefined) {
        op.setDefined(true, trigger);
    } else {
        swap(op.value, oldValue);
        op.changeValue([&]() {
            swap(op.value, oldValue);
            return trigger && wasDefined;
        });
    }
}

// returns true if the function did a full revaluate of the OpMinMax node
template <bool minMode>
inline bool handleOperandValueChange(OpMinMax<minMode>& op, Int index) {
    const ExprRef<IntView> expr =
        op.operand->view().template getMembers<IntView>()[index];
    bool fullReevaluate = false;
    if (op.compare(expr->view().value, op.value)) {
        op.value = expr->view().value;
        op.minValueIndices.clear();
        op.minValueIndices.insert(index);
    } else if (expr->view().value == op.value) {
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
class OperatorTrates<OpMinMax<minMode>>::OperandsSequenceTrigger
    : public SequenceTrigger {
   public:
    OpMinMax<minMode>* op;
    OperandsSequenceTrigger(OpMinMax<minMode>* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        bool exprDefined = !expr->isUndefined();

        if (exprDefined && (op->minValueIndices.empty() ||
                            op->compare(expr->view().value, op->value))) {
            op->minValueIndices.clear();
            op->minValueIndices.insert(index);
            if (!op->isDefined()) {
                op->value = expr->view().value;
                op->setDefined(!op->operand->isUndefined(), true);
            } else {
                op->changeValue([&]() {
                    op->value = expr->view().value;
                    return true;
                });
            }
        } else if (exprDefined && expr->view().value == op->value) {
            // value is equal to min value
            // need to add this index to set of min value indices.
            // must shift other indices up
            shiftIndicesUp(index, op->operand->view().numberElements(),
                           op->minValueIndices);
            op->minValueIndices.insert(index);
        } else if (!op->minValueIndices.empty()) {
            // expr is not less, so it is ignored, need to shift indices of mins
            // up though
            shiftIndicesUp(index, op->operand->view().numberElements(),
                           op->minValueIndices);
            return;
        }
    }

    void valueRemoved(UInt index, const AnyExprRef&) final {
        if (op->minValueIndices.count(index)) {
            op->minValueIndices.erase(index);
        }
        if (!op->minValueIndices.empty()) {
            shiftIndicesDown(index, op->operand->view().numberElements(),
                             op->minValueIndices);
        } else {
            updateMinValues(*op, true);
        }
    }
    inline void beginSwaps() final {}
    inline void endSwaps() final {}

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
    inline void possibleSubsequenceChange(UInt, UInt) final {}
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        op->changeValue([&]() {
            for (size_t i = startIndex; i < endIndex; i++) {
                bool fullReevaluated = handleOperandValueChange(*op, i);
                if (fullReevaluated) {
                    break;
                }
            }
            return true;
        });
    }
    void possibleValueChange() final {}
    void valueChanged() final {
        op->minValueIndices.clear();
        updateMinValues(*op, true);
    }
    void reattachTrigger() final {
        deleteTrigger(op->operandTrigger);
        auto trigger = make_shared<
            OperatorTrates<OpMinMax<minMode>>::OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }
    void hasBecomeUndefined() final { op->setDefined(false, true); }
    void hasBecomeDefined() final { op->setDefined(true, true); }
    void memberHasBecomeUndefined(UInt index) final {
        op->setDefined(false, true);
        if (op->minValueIndices.count(index)) {
            op->minValueIndices.erase(index);
        }
        if (op->minValueIndices.empty()) {
            updateMinValues(*op, true);
        }
    }

    void memberHasBecomeDefined(UInt index) final {
        auto& expr = op->operand->view().template getMembers<IntView>()[index];

        if ((op->minValueIndices.empty() ||
             op->compare(expr->view().value, op->value))) {
            op->minValueIndices.clear();
            op->minValueIndices.insert(index);
            op->value = expr->view().value;
            op->setDefined(!op->operand->isUndefined(), true);
        } else if (expr->view().value == op->value) {
            op->minValueIndices.insert(index);
        }
    }
};

template <bool minMode>
void OpMinMax<minMode>::updateViolationDescription(
    UInt parentViolation, ViolationDescription& vioDesc) {
    if (this->operand->isUndefined()) {
        this->operand->updateViolationDescription(parentViolation, vioDesc);
        return;
    }
    for (auto& operandChild :
         this->operand->view().template getMembers<IntView>()) {
        operandChild->updateViolationDescription(parentViolation, vioDesc);
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

template <typename Op>
struct OpMaker;

template <bool minMode>
struct OpMaker<OpMinMax<minMode>> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};

template <bool minMode>
ExprRef<IntView> OpMaker<OpMinMax<minMode>>::make(ExprRef<SequenceView> o) {
    return make_shared<OpMinMax<minMode>>(move(o));
}

template struct OpMinMax<true>;
template struct OpMaker<OpMinMax<true>>;
template struct OpMinMax<false>;
template struct OpMaker<OpMinMax<false>>;
;
