#include "operators/opSum.h"
#include <algorithm>
#include <cassert>
#include "operators/shiftViolatingIndices.h"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OpSum::OperandsSequenceTrigger;
void reevaluate(OpSum& op);
class OpSum::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    Int previousValue;
    OpSum* op;
    OperandsSequenceTrigger(OpSum* op) : op(op) {}
    void valueAdded(UInt, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        op->changeValue([&]() {
            op->value += expr->value;
            return true;
        });
    }

    void valueRemoved(UInt, const AnyExprRef& exprIn) final {
        const auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        op->changeValue([&]() {
            op->value -= expr->value;
            return true;
        });
    }

    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt, UInt) {}
    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        previousValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            previousValue +=
                op->operands->template getMembers<IntView>()[i]->value;
        }
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        Int newValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandValue =
                op->operands->template getMembers<IntView>()[i]->value;
            newValue += operandValue;
        }
        op->changeValue([&]() {
            op->value -= previousValue;
            op->value += newValue;
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
inline void reevaluate(OpSum& op) {
    op.value = 0;
    for (size_t i = 0; i < op.operands->numberElements(); ++i) {
        auto& operand = op.operands->template getMembers<IntView>()[i];
        op.value += operand->value;
    }
}

void OpSum::evaluate() {
    operands->evaluate();
    reevaluate(*this);
}

OpSum::OpSum(OpSum&& other)
    : IntView(move(other)),
      operands(move(other.operands)),
      operandsSequenceTrigger(move(other.operandsSequenceTrigger)) {
    setTriggerParent(this, operandsSequenceTrigger);
}

void OpSum::startTriggering() {
    if (!operandsSequenceTrigger) {
        operandsSequenceTrigger =
            std::make_shared<OperandsSequenceTrigger>(this);
        addTrigger(operands, operandsSequenceTrigger);
        operands->startTriggering();
    }
}

void OpSum::stopTriggering() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
        operands->stopTriggering();
    }
}

void OpSum::updateViolationDescription(UInt parentViolation,
                                       ViolationDescription& vioDesc) {
    for (auto& operand : operands->template getMembers<IntView>()) {
        operand->updateViolationDescription(parentViolation, vioDesc);
    }
}

ExprRef<IntView> OpSum::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newOpSum = make_shared<OpSum>(deepCopyForUnroll(operands, iterator));
    newOpSum->value = value;
    return ViewRef<IntView>(newOpSum);
}

std::ostream& OpSum::dumpState(std::ostream& os) const {
    os << "OpSum: value=" << value << endl;
    return operands->dumpState(os);
}

void OpSum::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->operands = findAndReplace(operands, func);
}
