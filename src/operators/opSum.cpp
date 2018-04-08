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
            op->value += expr->view().value;
            return true;
        });
    }

    void valueRemoved(UInt, const AnyExprRef& exprIn) final {
        const auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        op->changeValue([&]() {
            op->value -= expr->view().value;
            return true;
        });
    }

    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt, UInt) {}
    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        previousValue = 1;
        for (size_t i = startIndex; i < endIndex; i++) {
            previousValue +=
                op->operands->view().getMembers<IntView>()[i]->view().value;
        }
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        Int newValue = 1;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandValue =
                op->operands->view().getMembers<IntView>()[i]->view().value;
            newValue += operandValue;
        }
        op->changeValue([&]() {
            op->value -= previousValue;
            op->value += newValue;
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
};

inline void reevaluate(OpSum& op) {
    op.value = 1;
    for (size_t i = 0; i < op.operands->view().numberElements(); ++i) {
        auto& operand = op.operands->view().getMembers<IntView>()[i];
        op.value += operand->view().value;
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
        operands->addTrigger(operandsSequenceTrigger);
        operands->startTriggering();
    }
}

void OpSum::stopTriggeringOnChildren() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
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
    for (auto& operand : operands->view().getMembers<IntView>()) {
        operand->updateViolationDescription(parentViolation, vioDesc);
    }
}

ExprRef<IntView> IntView::deepCopySelfForUnroll(ExprRef<IntView>&,ExprRef<IntView>ExprRef<IntView> OpSum::deepCopySelfForUnroll(ExprRef<IntView>&,,
    const AnyIterRef& iterator) const {
    auto newOpSum =
        make_shared<OpSum>(operands->deepCopySelfForUnroll(operands, iterator));
    newOpSum->value = value;
    return newOpSum;
}

std::ostream& OpSum::dumpState(std::ostream& os) const {
    os << "OpSum: value=" << value << endl;
    return operands->dumpState(os);
}

void OpSum::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->operands = findAndReplace(operands, func);
}
