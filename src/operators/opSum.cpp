#include "operators/opSum.h"
#include <algorithm>
#include <cassert>
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OperatorTrates<OpSum>::OperandsSequenceTrigger;
class OperatorTrates<OpSum>::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    Int previousValue;
    OpSum* op;
    OperandsSequenceTrigger(OpSum* op) : op(op) {}
    void valueAdded(UInt, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        if (expr->isUndefined()) {
            if (op->operand->view().numberUndefined == 1) {
                op->setDefined(false, false);
                visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                              op->triggers);
            }
            return;
        }
        op->changeValue([&]() {
            op->value += expr->view().value;
            return true;
        });
    }

    void valueRemoved(UInt, const AnyExprRef& exprIn) final {
        const auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        if (expr->isUndefined()) {
            if (op->operand->view().numberUndefined == 0) {
                op->setDefined(true, false);
                visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                              op->triggers);
            }
            return;
        }
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
        previousValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            debug_code(assert(
                !op->operand->view().getMembers<IntView>()[i]->isUndefined()));
            previousValue +=
                op->operand->view().getMembers<IntView>()[i]->view().value;
        }
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        Int newValue = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandValue =
                op->operand->view().getMembers<IntView>()[i]->view().value;
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
            op->reevaluate();
            return true;
        });
    }
    void reattachTrigger() final {
        deleteTrigger(op->operandTrigger);
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }
    void hasBecomeUndefined() final { op->setDefined(false, true); }
    void hasBecomeDefined() final { op->setDefined(true, true); }

    void memberHasBecomeUndefined(UInt) {
        op->value -= previousValue;
        if (op->operand->view().numberUndefined == 1) {
            op->setDefined(false, false);
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        }
    }

    void memberHasBecomeDefined(UInt index) {
        op->value +=
            op->operand->view().getMembers<IntView>()[index]->view().value;
        if (op->operand->view().numberUndefined == 0) {
            op->setDefined(true, false);
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                          op->triggers);
        }
    }
};

void OpSum::reevaluate() {
    value = 0;
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        value += operandChild->view().value;
    }
}

void OpSum::updateViolationDescription(UInt parentViolation,
                                       ViolationDescription& vioDesc) {
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        operandChild->updateViolationDescription(parentViolation, vioDesc);
    }
}

void OpSum::copy(OpSum& newOp) const { newOp.value = value; }

std::ostream& OpSum::dumpState(std::ostream& os) const {
    os << "OpSum: value=" << value << endl;
    return operand->dumpState(os);
}
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSum> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};

ExprRef<IntView> OpMaker<OpSum>::make(ExprRef<SequenceView> o) {
    return make_shared<OpSum>(move(o));
}
