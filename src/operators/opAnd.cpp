#include "operators/opAnd.h"
#include <algorithm>
#include <cassert>
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OpAnd::OperandsSequenceTrigger;
void reevaluate(OpAnd& op);
class OpAnd::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    UInt previousViolation;
    OpAnd* op;
    UInt lastViolation = 0;
    OperandsSequenceTrigger(OpAnd* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        for (size_t i = op->operands->numberElements() - 1; i > index + 1;
             i--) {
            if (op->violatingOperands.erase(i - 1)) {
                op->violatingOperands.insert(i);
            }
        }
        UInt violation = expr->violation;
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
        UInt violationOfRemovedExpr = expr->violation;
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        op->violatingOperands.erase(index);
        for (size_t i = index; i < op->operands->numberElements() - 1; i++) {
            if (op->violatingOperands.erase(index + 1)) {
                op->violatingOperands.insert(index);
            }
        }
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
            previousViolation +=
                op->operands->template getMembers<BoolView>()[i]->violation;
        }
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        UInt newViolation = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandViolation =
                op->operands->template getMembers<BoolView>()[i]->violation;
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
    void possibleSequenceValueChange() final {}
    void sequenceValueChanged() final {
        op->changeValue([&]() {
            reevaluate(*op);
            return true;
        });
    }
    inline void preIterValueChange(const ExprRef<SequenceView>&) final {}
    inline void postIterValueChange(const ExprRef<SequenceView>&) final {
        sequenceValueChanged();
    }
};
inline void reevaluate(OpAnd& op) {
    op.violation = 0;
    for (size_t i = 0; i < op.operands->numberElements(); ++i) {
        auto& operand = op.operands->template getMembers<BoolView>()[i];
        if (operand->violation > 0) {
            op.violatingOperands.insert(i);
        }
        op.violation += operand->violation;
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
        addTrigger(operands, operandsSequenceTrigger);
        operands->startTriggering();
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
        operands->getMembers<BoolView>()[violatingOperandIndex]
            ->updateViolationDescription(violation, vioDesc);
    }
}

ExprRef<BoolView> OpAnd::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpAnd = make_shared<OpAnd>(deepCopyForUnroll(operands, iterator));
    newOpAnd->violation = violation;
    newOpAnd->violatingOperands = violatingOperands;
    return ViewRef<BoolView>(newOpAnd);
}

std::ostream& OpAnd::dumpState(std::ostream& os) const {
    os << "OpAnd: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                         violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
    return operands->dumpState(os);
}
