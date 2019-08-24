#include "operators/opSum.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/flatten.h"
#include "operators/previousValueCache.h"
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "types/intVal.h"
#include "types/sequenceVal.h"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OperatorTrates<OpSum>::OperandsSequenceTrigger;

void OpSum::addSingleValue(Int exprValue) { value += exprValue; }
void OpSum::removeSingleValue(Int exprValue) { value -= exprValue; }

class OperatorTrates<OpSum>::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    OpSum* op;
    OperandsSequenceTrigger(OpSum* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        debug_log("value added, index=" << index << " value=" << exprIn);
        if (!op->evaluationComplete) {
            return;
        }
        auto& expr = mpark::get<ExprRef<IntView>>(exprIn);
        auto view = expr->getViewIfDefined();
        if (!view) {
            op->cachedValues.insert(index, 0);
            if (op->operand->view()->numberUndefined == 1) {
                op->setUndefinedAndTrigger();
            }
            return;
        }
        Int operandValue = (*view).value;
        op->cachedValues.insert(index, operandValue);
        op->changeValue([&]() {
            op->addSingleValue(operandValue);
            return true;
        });
    }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        if (!op->evaluationComplete) {
            return;
        }
        const auto& expr = mpark::get<ExprRef<IntView>>(exprIn);

        Int operandValue = op->cachedValues.erase(index);

        if (!expr->appearsDefined()) {
            if (op->operand->view()->numberUndefined == 0) {
                op->setDefinedAndTrigger();
            }
            return;
        }
        op->changeValue([&]() {
            op->removeSingleValue(operandValue);
            return true;
        });
    }

    inline void positionsSwapped(UInt index1, UInt index2) {
        if (!op->evaluationComplete) {
            return;
        }

        swap(op->cachedValues.get(index1), op->cachedValues.get(index2));
    }

    ExprRef<IntView>& getMember(SequenceView& operandView, UInt index) {
        return operandView.getMembers<IntView>()[index];
    }
    Int getValueCatchUndef(SequenceView& operandView, UInt index) {
        auto& member = getMember(operandView, index);
        auto view = member->getViewIfDefined();
        return (view) ? (*view).value : 0;
    }

    inline void handleSingleOperandChange(SequenceView& operandView,
                                          UInt index) {
        Int newValue = getValueCatchUndef(operandView, index);
        Int oldValue = op->cachedValues.getAndSet(index, newValue);
        op->removeSingleValue(oldValue);
        op->addSingleValue(newValue);
    }

    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        if (!op->evaluationComplete) {
            return;
        }
        auto view = op->operand->view();
        if (!view) {
            hasBecomeUndefined();
            return;
        }
        auto& operandView = *view;
        op->changeValue([&]() {
            for (size_t i = startIndex; i < endIndex; i++) {
                handleSingleOperandChange(operandView, i);
            }
            return op->isDefined();
        });
    }

    void valueChanged() final {
        bool wasDefined = op->isDefined();
        op->changeValue([&]() {
            op->reevaluate();
            return op->isDefined();
        });
        if (wasDefined && !op->isDefined()) {
            op->notifyValueUndefined();
        } else if (!wasDefined && op->isDefined()) {
            op->notifyValueDefined();
        }
    }

    void reattachTrigger() final {
        deleteTrigger(op->operandTrigger);
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }

    void hasBecomeUndefined() final {
        op->setUndefinedAndTrigger();
        op->evaluationComplete = false;
    }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }

    void memberHasBecomeUndefined(UInt index) {
        if (!op->evaluationComplete) {
            return;
        }
        op->removeSingleValue(op->cachedValues.get(index));
        op->cachedValues.set(index, 0);
        auto view = op->operand->view();
        if (!view) {
            hasBecomeUndefined();
            return;
        }
        if ((*view).numberUndefined == 1) {
            op->setUndefinedAndTrigger();
        }
    }

    void memberHasBecomeDefined(UInt index) {
        auto operandView = op->operand->view();
        if (!operandView) {
            hasBecomeUndefined();
            return;
        }
        if (!op->evaluationComplete) {
            if ((*operandView).numberUndefined == 0) {
                op->reevaluateDefinedAndTrigger();
            }
            return;
        }

        Int operandValue = getValueCatchUndef(*operandView, index);
        op->addSingleValue(operandValue);
        op->cachedValues.set(index, operandValue);
        if ((*operandView).numberUndefined == 0) {
            op->setDefinedAndTrigger();
        }
    }
};

void OpSum::reevaluateImpl(SequenceView& operandView) {
    setDefined(true);
    value = 0;
    cachedValues.clear();
    auto& members = operandView.getMembers<IntView>();
    for (size_t index = 0; index < members.size(); index++) {
        auto& operandChild = members[index];
        auto operandChildView = operandChild->getViewIfDefined();
        if (!operandChildView) {
            setDefined(false);
            cachedValues.insert(index, 0);
        } else {
            Int operandValue = (*operandChildView).value;
            addSingleValue(operandValue);
            cachedValues.insert(index, operandValue);
        }
    }
    evaluationComplete = true;
}

void OpSum::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    auto operandView = operand->view();
    if (!operandView) {
        operand->updateVarViolations(vioContext, vioContainer);
        return;
    }
    for (auto& operandChild : (*operandView).getMembers<IntView>()) {
        operandChild->updateVarViolations(vioContext, vioContainer);
    }
}

void OpSum::copy(OpSum& newOp) const {
    newOp.value = value;
    newOp.cachedValues = cachedValues;
    newOp.evaluationComplete = this->evaluationComplete;
}

std::ostream& OpSum::dumpState(std::ostream& os) const {
    os << "OpSum: value=" << value << endl;
    return operand->dumpState(os);
}

std::pair<bool, ExprRef<IntView>> OpSum::optimiseImpl(ExprRef<IntView>& self,
                                                       PathExtension path) {
    auto boolOpPair = standardOptimise(self, path);
    boolOpPair.first |= flatten<IntView>(*(boolOpPair.second));
    return boolOpPair;
}
string OpSum::getOpName() const { return "OpSum"; }

void OpSum::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto viewOption = operand->view();
    if (!viewOption) {
        return;
    }
    auto& operandView = *viewOption;
    Int checkValue = 0;
    auto& members = operandView.getMembers<IntView>();
    for (size_t index = 0; index < members.size(); index++) {
        auto& operandChild = members[index];
        auto operandChildView = operandChild->getViewIfDefined();
        sanityCheck(index < cachedValues.size(), "cachedValues too small");
        if (!operandChildView) {
            sanityEqualsCheck(0, cachedValues.get(index));
        } else {
            Int operandValue = operandChildView->value;
            checkValue += operandValue;
            sanityEqualsCheck(operandValue, cachedValues.get(index));
        }
    }
    sanityEqualsCheck(checkValue, value);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSum> {
    static ExprRef<IntView> make(
        ExprRef<SequenceView>,
        const shared_ptr<SequenceDomain>& operandDomain = nullptr);
};

ExprRef<IntView> OpMaker<OpSum>::make(
    ExprRef<SequenceView> o, const shared_ptr<SequenceDomain>& operandDomain) {
    if (operandDomain &&
        mpark::get_if<shared_ptr<EmptyDomain>>(&operandDomain->inner)) {
        auto val = ::make<IntValue>();
        val->value = 0;
        val->setConstant(true);
        return val.asExpr();
    }

    return make_shared<OpSum>(move(o));
}
