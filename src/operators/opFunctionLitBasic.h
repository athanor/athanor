#ifndef SRC_OPERATORS_OPFUNCTIONLITBASIC_H_
#define SRC_OPERATORS_OPFUNCTIONLITBASIC_H_
#include "base/base.h"
#include "types/function.h"

struct OpFunctionLitBasic : public FunctionView {
    struct ExprTriggerBase {
        OpFunctionLitBasic* op;
        UInt index;
        ExprTriggerBase(OpFunctionLitBasic* op, UInt index)
            : op(op), index(index) {}
    };
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;

    OpFunctionLitBasic(AnyDomainRef preImageDomain, AnyExprVec rangeIn)
        : FunctionView(std::move(preImageDomain), std::move(rangeIn)) {}
    OpFunctionLitBasic() {}
    OpFunctionLitBasic(const OpFunctionLitBasic&) = delete;
    OpFunctionLitBasic(OpFunctionLitBasic&&) = delete;
    ~OpFunctionLitBasic() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<FunctionView> deepCopyForUnrollImpl(
        const ExprRef<FunctionView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<FunctionView>> optimiseImpl(
        ExprRef<FunctionView>&, PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    AnyExprVec& getChildrenOperands() final;
};

#endif /* SRC_OPERATORS_OPFUNCTIONLITBASIC_H_ */
