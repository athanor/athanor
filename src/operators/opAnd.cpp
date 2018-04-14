#include "operators/opAnd.h"
#include <algorithm>
#include <cassert>
#include "operators/shiftViolatingIndices.h"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OpAnd::OperandsSequenceTrigger;
void reevaluate(OpAnd& op);
class OpAnd::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    UInt previousViolation;
    OpAnd* op;
    OperandsSequenceTrigger(OpAnd* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        shiftIndicesUp(index, op->operands->view().numberElements(),
                       op->violatingOperands);
        UInt violation = expr->view().violation;
        if (violation > 0) {
            op->violatingOperands.insert(index);
            op->changeValue([&]() {
                op->violation += violation;
                return true;
            });
        }
    }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        const auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        UInt violationOfRemovedExpr = expr->view().violation;
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        op->violatingOperands.erase(index);
        shiftIndicesDown(index, op->operands->view().numberElements(),
                         op->violatingOperands);
        op->changeValue([&]() {
            op->violation -= violationOfRemovedExpr;
            return true;
        });
    }

    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt index1, UInt index2) {
        if (op->violatingOperands.count(index1)) {
            if (!op->violatingOperands.count(index2)) {
                op->violatingOperands.erase(index1);
                op->violatingOperands.insert(index2);
            }
        } else {
            if (op->violatingOperands.count(index2)) {
                op->violatingOperands.erase(index2);
                op->violatingOperands.insert(index1);
            }
        }
    }
    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        previousViolation = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            previousViolation += op->operands->view()
                                     .getMembers<BoolView>()[i]
                                     ->view()
                                     .violation;
        }
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        UInt newViolation = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandViolation = op->operands->view()
                                        .getMembers<BoolView>()[i]
                                        ->view()
                                        .violation;
            newViolation += operandViolation;
            if (operandViolation > 0) {
                op->violatingOperands.insert(i);
            } else {
                op->violatingOperands.erase(i);
            }
        }
        op->changeValue([&]() {
            op->violation -= previousViolation;
            op->violation += newViolation;
            return true;
        });
    }
    void possibleValueChange() final {}
    void valueChanged() final {
        op->changeValue([&]() {
            reevaluate(*op);
            return true;
        });
    }

    void reattachTrigger() final {
        deleteTrigger(op->operandsSequenceTrigger);
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operands->addTrigger(trigger);
        op->operandsSequenceTrigger = trigger;
    }
};
inline void reevaluate(OpAnd& op) {
    op.violation = 0;
    for (size_t i = 0; i < op.operands->view().numberElements(); ++i) {
        auto& operand = op.operands->view().getMembers<BoolView>()[i];
        if (operand->view().violation > 0) {
            op.violatingOperands.insert(i);
        }
        op.violation += operand->view().violation;
    }
}

void OpAnd::evaluate() {
    operands->evaluate();
    reevaluate(*this);
}

OpAnd::OpAnd(OpAnd&& other)
    : BoolView(move(other)),
      operands(move(other.operands)),
      violatingOperands(move(other.violatingOperands)),
      operandsSequenceTrigger(move(other.operandsSequenceTrigger)) {
    setTriggerParent(this, operandsSequenceTrigger);
}

void OpAnd::startTriggering() {
    if (!operandsSequenceTrigger) {
        operandsSequenceTrigger =
            std::make_shared<OperandsSequenceTrigger>(this);
        operands->addTrigger(operandsSequenceTrigger);
        operands->startTriggering();
    }
}

void OpAnd::stopTriggeringOnChildren() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
    }
}

void OpAnd::stopTriggering() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
        operands->stopTriggering();
    }
}

void OpAnd::updateViolationDescription(const UInt,
                                       ViolationDescription& vioDesc) {
    for (size_t violatingOperandIndex : violatingOperands) {
        operands->view()
            .getMembers<BoolView>()[violatingOperandIndex]
            ->updateViolationDescription(violation, vioDesc);
    }
}

ExprRef<BoolView> OpAnd::deepCopySelfForUnroll(
    const ExprRef<BoolView>&, const AnyIterRef& iterator) const {
    auto newOpAnd =
        make_shared<OpAnd>(operands->deepCopySelfForUnroll(operands, iterator));
    newOpAnd->violation = violation;
    newOpAnd->violatingOperands = violatingOperands;
    return newOpAnd;
}

std::ostream& OpAnd::dumpState(std::ostream& os) const {
    os << "OpAnd: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                         violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
    return operands->dumpState(os);
}

void OpAnd::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->operands = findAndReplace(operands, func);
}
