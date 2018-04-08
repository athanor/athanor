#include "operators/opProd.h"
#include <algorithm>
#include <cassert>
#include "operators/shiftViolatingIndices.h"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OpProd::OperandsSequenceTrigger;
void reevaluate(OpProd& op);
class OpProd::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    Int previousValue;
    OpProd* op;
    OperandsSequenceTrigger(OpProd* op) : op(op) {}
    void valueAdded(UInt, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        op->changeValue([&]() {
            op->value *= expr->view().value;
            return true;
        });
    }

    void valueRemoved(UInt, const AnyExprRef& exprIn) final {
        const auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        op->changeValue([&]() {
            op->value /= expr->view().value;
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
            previousValue *=
                op->operands->view().getMembers<IntView>()[i]->view().value;
        }
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        Int newValue = 1;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandValue =
                op->operands->view().getMembers<IntView>()[i]->view().value;
            newValue *= operandValue;
        }
        op->changeValue([&]() {
            op->value /= previousValue;
            op->value *= newValue;
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

inline void reevaluate(OpProd& op) {
    op.value = 1;
    for (size_t i = 0; i < op.operands->view().numberElements(); ++i) {
        auto& operand = op.operands->view().getMembers<IntView>()[i];
        op.value *= operand->view().value;
    }
}

void OpProd::evaluate() {
    operands->evaluate();
    reevaluate(*this);
}

OpProd::OpProd(OpProd&& other)
    : IntView(move(other)),
      operands(move(other.operands)),
      operandsSequenceTrigger(move(other.operandsSequenceTrigger)) {
    setTriggerParent(this, operandsSequenceTrigger);
}

void OpProd::startTriggering() {
    if (!operandsSequenceTrigger) {
        operandsSequenceTrigger =
            std::make_shared<OperandsSequenceTrigger>(this);
        operands->addTrigger(operandsSequenceTrigger);
        operands->startTriggering();
    }
}

void OpProd::stopTriggeringOnChildren() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
    }
}

void OpProd::stopTriggering() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
        operands->stopTriggering();
    }
}

void OpProd::updateViolationDescription(UInt parentViolation,
                                        ViolationDescription& vioDesc) {
    for (auto& operand : operands->view().getMembers<IntView>()) {
        operand->updateViolationDescription(parentViolation, vioDesc);
    }
}

ExprRef<IntView> IntView::deepCopySelfForUnroll(const ExprRef<IntView>&,ExprRef<IntView>ExprRef<IntView> OpProd::deepCopySelfForUnroll(const ExprRef<IntView>&,,
    const AnyIterRef& iterator) const {
    auto newOpProd = make_shared<OpProd>(
        operands->deepCopySelfForUnroll(operands, iterator));
    newOpProd->value = value;
    return newOpProd;
}

std::ostream& OpProd::dumpState(std::ostream& os) const {
    os << "OpProd: value=" << value << endl;
    return operands->dumpState(os);
}

void OpProd::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->operands = findAndReplace(operands, func);
}
