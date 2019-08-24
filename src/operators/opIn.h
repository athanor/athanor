#ifndef SRC_OPERATORS_OPIN_H_
#define SRC_OPERATORS_OPIN_H_
#include "base/base.h"
#include "types/bool.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"

struct OpIn : public BoolView {
    struct ExprTriggerBase : public virtual TriggerBase {
        OpIn* op;
        ExprTriggerBase(OpIn* op) : op(op) {}
    };
    struct SetOperandTrigger;
    AnyExprRef expr;
    ExprRef<SetView> setOperand;
    std::shared_ptr<ExprTriggerBase> exprTrigger;
    std::shared_ptr<SetOperandTrigger> setOperandTrigger;
    OpIn(AnyExprRef expr, ExprRef<SetView> setOperand)
        : expr(std::move(expr)), setOperand(std::move(setOperand)) {}
    OpIn(const OpIn&) = delete;
    OpIn(OpIn&&) = delete;
    ~OpIn() { this->stopTriggeringOnChildren(); }
    void reevaluate();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<BoolView> deepCopyForUnrollImpl(
        const ExprRef<BoolView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<BoolView>> optimiseImpl(ExprRef<BoolView>&,
                                                    PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPIN_H_ */
