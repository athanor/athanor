#include "operators/opIn.h"
#include <iostream>
#include <memory>
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;
#define invoke(expr, func) mpark::visit([&](auto& expr) { return func; }, expr)

#define invoke_r(expr, func, ret) \
    mpark::visit([&](auto& expr) -> ret { return func; }, expr)

void OpIn::reevaluate() {
    if (setOperand->isUndefined() || invoke(expr, expr->isUndefined())) {
        violation = LARGE_VIOLATION;
        return;
    }
    auto hash = invoke_r(expr, getValueHash(expr->view()), HashType);
    violation = (setOperand->view().memberHashes.count(hash)) ? 0 : 1;
}

void OpIn::evaluateImpl() {
    invoke(expr, expr->evaluate());
    setOperand->evaluate();
    reevaluate();
}

OpIn::OpIn(OpIn&& other)
    : BoolView(move(other)),
      expr(move(other.expr)),
      setOperand(move(other.setOperand)),
      exprTrigger(move(other.exprTrigger)) {
    setTriggerParent(this, exprTrigger, setOperandTrigger);
}
namespace {

template <typename Derived, typename TriggerType>
struct Trigger
    : public ChangeTriggerAdapter<TriggerType, Trigger<Derived, TriggerType>>,
      public OpIn::ExprTriggerBase {
    using OpIn::ExprTriggerBase::ExprTriggerBase;
    void adapterPossibleValueChange() {}
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
template <typename ExprTriggerType>
struct ExprTrigger
    : public Trigger<ExprTrigger<ExprTriggerType>, ExprTriggerType> {
    using Trigger<ExprTrigger<ExprTriggerType>, ExprTriggerType>::Trigger;
    void reattachTrigger() {
        deleteTrigger(this->op->exprTrigger);
        typedef typename AssociatedViewType<ExprTriggerType>::type View;
        auto& expr = mpark::get<ExprRef<View>>(this->op->expr);
        auto trigger = make_shared<ExprTrigger<ExprTriggerType>>(this->op);
        expr->addTrigger(trigger);
        this->op->exprTrigger = trigger;
    }
};
}  // namespace

struct OpIn::SetOperandTrigger : public Trigger<SetOperandTrigger, SetTrigger> {
    using Trigger<SetOperandTrigger, SetTrigger>::Trigger;
    void reattachTrigger() final {
        deleteTrigger(op->setOperandTrigger);
        auto trigger = make_shared<SetOperandTrigger>(op);
        op->setOperand->addTrigger(trigger);
        op->setOperandTrigger = trigger;
    }
};

void OpIn::startTriggeringImpl() {
    if (!exprTrigger) {
        mpark::visit(
            [&](auto& expr) {
                typedef typename AssociatedTriggerType<viewType(expr)>::type
                    TriggerType;
                auto trigger = make_shared<ExprTrigger<TriggerType>>(this);
                exprTrigger = trigger;
                expr->addTrigger(trigger);
                expr->startTriggering();
            },
            expr);
        setOperandTrigger = make_shared<SetOperandTrigger>(this);
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

void OpIn::updateVarViolations(const ViolationContext&,
                               ViolationContainer& vioDesc) {
    if (violation == 0) {
        return;
    } else {
        invoke(expr, expr->updateVarViolations(violation, vioDesc));
        setOperand->updateVarViolations(violation, vioDesc);
    }
}

ExprRef<BoolView> OpIn::deepCopySelfForUnrollImpl(
    const ExprRef<BoolView>&, const AnyIterRef& iterator) const {
    auto newOpIn = make_shared<OpIn>(
        invoke_r(expr, expr->deepCopySelfForUnroll(expr, iterator), AnyExprRef),
        setOperand->deepCopySelfForUnroll(setOperand, iterator));
    newOpIn->violation = violation;
    return newOpIn;
}

std::ostream& OpIn::dumpState(std::ostream& os) const {
    os << "opIn(expr=";
    invoke_r(expr, expr->dumpState(os), ostream&) << ",\n";
    os << "setOperand=";
    setOperand->dumpState(os);
    return os;
}

void OpIn::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    invoke(expr, expr->findAndReplaceSelf(func));
    setOperand->findAndReplaceSelf(func);
}

bool OpIn::optimise(PathExtension path) {
    bool changeMade = false;
    changeMade |= invoke(expr, expr->optimise(path.extend(expr)));
    changeMade |= setOperand->optimise(path.extend(setOperand));
    return changeMade;
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
