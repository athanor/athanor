#include "operators/opProd.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OperatorTrates<OpProd>::OperandsSequenceTrigger;
class OperatorTrates<OpProd>::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    Int previousValue;
    unordered_map<UInt, Int> previousValues;
    OpProd* op;
    OperandsSequenceTrigger(OpProd* op) : op(op) {}
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
            op->value *= expr->view().value;
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
            op->value /= expr->view().value;
            return true;
        });
    }

    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt, UInt) {}
    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        if (endIndex - startIndex == 1) {
            previousValues[startIndex] = op->operand->view()
                                             .getMembers<IntView>()[startIndex]
                                             ->view()
                                             .value;
            return;
        }

        previousValue = 1;
        for (size_t i = startIndex; i < endIndex; i++) {
            debug_code(assert(
                !op->operand->view().getMembers<IntView>()[i]->isUndefined()));
            previousValue *=
                op->operand->view().getMembers<IntView>()[i]->view().value;
        }
    }

    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        Int newValue = 1;
        Int valueToRemove;
        if (endIndex - startIndex == 1) {
            debug_code(assert(previousValues.count(startIndex)));
            valueToRemove = previousValues[startIndex];
            previousValues.erase(startIndex);
        } else {
            valueToRemove = previousValue;
        }

        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandValue =
                op->operand->view().getMembers<IntView>()[i]->view().value;
            newValue *= operandValue;
        }
        op->changeValue([&]() {
            op->value /= valueToRemove;
            op->value *= newValue;
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
        op->value /= previousValue;
        if (op->operand->view().numberUndefined == 1) {
            op->setDefined(false, false);
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        }
    }

    void memberHasBecomeDefined(UInt index) {
        op->value *=
            op->operand->view().getMembers<IntView>()[index]->view().value;
        if (op->operand->view().numberUndefined == 0) {
            op->setDefined(true, false);
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                          op->triggers);
        }
    }
};

void OpProd::reevaluate() {
    value = 1;
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        value *= operandChild->view().value;
    }
}

void OpProd::updateViolationDescription(UInt parentViolation,
                                        ViolationDescription& vioDesc) {
    for (auto& operandChild : operand->view().getMembers<IntView>()) {
        operandChild->updateViolationDescription(parentViolation, vioDesc);
    }
}

void OpProd::copy(OpProd& newOp) const { newOp.value = value; }

std::ostream& OpProd::dumpState(std::ostream& os) const {
    os << "OpProd: value=" << value << endl;
    return operand->dumpState(os);
}
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpProd> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};

ExprRef<IntView> OpMaker<OpProd>::make(ExprRef<SequenceView> o) {
    return make_shared<OpProd>(move(o));
}
