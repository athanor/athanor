#include "operators/opIn.h"

#include <iostream>
#include <memory>

#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;
#define invoke(expr, func) lib::visit([&](auto& expr) { return func; }, expr)

#define invoke_r(expr, func, ret) \
    lib::visit([&](auto& expr) -> ret { return func; }, expr)

void OpIn::reevaluate() {
    lib::visit(
        [&](auto& expr) {
            auto exprView = expr->getViewIfDefined();
            auto setView = setOperand->getViewIfDefined();
            if (!exprView || !setView) {
                violation = LARGE_VIOLATION;
                return;
            }
            HashType hash = getValueHash(*exprView);
            violation = ((*setView).hashIndexMap.count(hash)) ? 0 : 1;
        },
        expr);
}

void OpIn::evaluateImpl() {
    invoke(expr, expr->evaluate());
    setOperand->evaluate();
    reevaluate();
}

namespace {

template <typename Derived, typename TriggerType>
struct Trigger : public ChangeTriggerAdapter<TriggerType, Derived>,
                 public OpIn::ExprTriggerBase {
    typedef typename AssociatedViewType<TriggerType>::type OperandType;
    Trigger(OpIn* op, ExprRef<OperandType> operand)
        : ChangeTriggerAdapter<TriggerType, Derived>(operand),
          OpIn::ExprTriggerBase(op) {}
    void adapterValueChanged() {
        op->changeValue([&]() {
            op->reevaluate();
            return true;
        });
    }
    void adapterHasBecomeUndefined() {
        op->changeValue([&]() {
            op->reevaluate();
            return true;
        });
    }
    void adapterHasBecomeDefined() {
        op->changeValue([&]() {
            op->reevaluate();
            return true;
        });
    }
};
}  // namespace

template <typename ExprTriggerType>
struct ExprTrigger
    : public Trigger<ExprTrigger<ExprTriggerType>, ExprTriggerType> {
    typedef typename AssociatedViewType<ExprTriggerType>::type ExprType;
    using Trigger<ExprTrigger<ExprTriggerType>, ExprTriggerType>::Trigger;
    ExprRef<ExprType>& getTriggeringOperand() {
        return lib::get<ExprRef<ExprType>>(this->op->expr);
    }
    void reattachTrigger() {
        deleteTrigger(this->op->exprTrigger);
        auto& expr = lib::get<ExprRef<ExprType>>(this->op->expr);
        auto trigger = make_shared<ExprTrigger<ExprTriggerType>>(
            this->op, getTriggeringOperand());
        expr->addTrigger(trigger);
        this->op->exprTrigger = trigger;
    }
};

struct OpIn::SetOperandTrigger : public Trigger<SetOperandTrigger, SetTrigger> {
    using Trigger<SetOperandTrigger, SetTrigger>::Trigger;
    ExprRef<SetView>& getTriggeringOperand() { return this->op->setOperand; }

    void reattachTrigger() final {
        deleteTrigger(this->op->setOperandTrigger);
        auto trigger =
            make_shared<SetOperandTrigger>(this->op, getTriggeringOperand());
        this->op->setOperand->addTrigger(trigger);
        this->op->setOperandTrigger = trigger;
    }
};

void OpIn::startTriggeringImpl() {
    if (!exprTrigger) {
        lib::visit(
            [&](auto& expr) {
                typedef typename AssociatedTriggerType<viewType(expr)>::type
                    TriggerType;
                auto trigger =
                    make_shared<ExprTrigger<TriggerType>>(this, expr);
                exprTrigger = trigger;
                expr->addTrigger(trigger);
                expr->startTriggering();
            },
            expr);
        setOperandTrigger = make_shared<SetOperandTrigger>(this, setOperand);
        setOperand->addTrigger(setOperandTrigger);
        setOperand->startTriggering();
    }
}

void OpIn::stopTriggeringOnChildren() {
    if (exprTrigger) {
        deleteTrigger(exprTrigger);
        deleteTrigger(setOperandTrigger);
        exprTrigger = nullptr;
        setOperandTrigger = nullptr;
    }
}

void OpIn::stopTriggering() {
    if (exprTrigger) {
        stopTriggeringOnChildren();
        invoke(expr, expr->stopTriggering());
        setOperand->stopTriggering();
    }
}

void OpIn::updateVarViolationsImpl(const ViolationContext&,
                                   ViolationContainer& vioContainer) {
    if (violation == 0) {
        return;
    } else {
        invoke(expr, expr->updateVarViolations(violation, vioContainer));
        setOperand->updateVarViolations(violation, vioContainer);
    }
}

ExprRef<BoolView> OpIn::deepCopyForUnrollImpl(
    const ExprRef<BoolView>&, const AnyIterRef& iterator) const {
    auto newOpIn = make_shared<OpIn>(
        invoke_r(expr, expr->deepCopyForUnroll(expr, iterator), AnyExprRef),
        setOperand->deepCopyForUnroll(setOperand, iterator));
    return newOpIn;
}

std::ostream& OpIn::dumpState(std::ostream& os) const {
    os << "opIn(expr=";
    invoke_r(expr, expr->dumpState(os), ostream&) << ",\n";
    os << "setOperand=";
    setOperand->dumpState(os);
    return os;
}

void OpIn::findAndReplaceSelf(const FindAndReplaceFunction& func,
                              PathExtension path) {
    expr = invoke_r(expr, findAndReplace(expr, func, path), AnyExprRef);
    setOperand = findAndReplace(setOperand, func, path);
    ;
}

pair<bool, ExprRef<BoolView>> OpIn::optimiseImpl(ExprRef<BoolView>&,
                                                 PathExtension path) {
    auto newOp = make_shared<OpIn>(expr, setOperand);
    AnyExprRef newOpAsExpr = ExprRef<BoolView>(newOp);
    bool optimised = false;
    lib::visit(
        [&](auto& expr) { optimised |= optimise(newOpAsExpr, expr, path); },
        newOp->expr);
    optimised |= optimise(newOpAsExpr, newOp->setOperand, path);
    return make_pair(optimised, newOp);
}

string OpIn::getOpName() const { return "OpIn"; }
void OpIn::debugSanityCheckImpl() const {
    bool exprDefined = lib::visit(
        [&](auto& expr) {
            expr->debugSanityCheck();
            return expr->getViewIfDefined().hasValue();
        },
        expr);
    setOperand->debugSanityCheck();
    auto setView = setOperand->getViewIfDefined();

    if (!exprDefined || !setView) {
        sanityLargeViolationCheck(violation);
        return;
    }
    HashType hash = getValueHash(expr);
    if (setView->hashIndexMap.count(hash)) {
        sanityEqualsCheck(0, violation);
    } else {
        sanityEqualsCheck(1, violation);
    }
}
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpIn> {
    static ExprRef<BoolView> make(AnyExprRef expr, ExprRef<SetView> setOperand);
};

ExprRef<BoolView> OpMaker<OpIn>::make(AnyExprRef expr,
                                      ExprRef<SetView> setOperand) {
    return make_shared<OpIn>(move(expr), move(setOperand));
}
