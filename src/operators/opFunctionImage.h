
#ifndef SRC_OPERATORS_OPFUNCTIONIMAGE_H_
#define SRC_OPERATORS_OPFUNCTIONIMAGE_H_

#include "base/base.h"
#include "types/function.h"
#include "types/int.h"
template <typename FunctionMemberViewType>
struct OpFunctionImage : public ExprInterface<FunctionMemberViewType> {
    struct PreImageTriggerBase {
        OpFunctionImage<FunctionMemberViewType>* op;
        PreImageTriggerBase(OpFunctionImage<FunctionMemberViewType>* op)
            : op(op) {}
        virtual ~PreImageTriggerBase() {}
    };

    typedef typename AssociatedTriggerType<FunctionMemberViewType>::type
        FunctionMemberTriggerType;
    std::vector<std::shared_ptr<TriggerBase>> triggers;
    ExprRef<FunctionView> functionOperand;
    AnyExprRef preImageOperand;
    Int cachedIndex;
    bool locallyDefined = false;
    std::shared_ptr<PreImageTriggerBase> preImageTrigger;
    OpFunctionImage(ExprRef<FunctionView> functionOperand,
                    AnyExprRef preImageOperand)
        : functionOperand(std::move(functionOperand)),
          preImageOperand(std::move(preImageOperand)) {}
    OpFunctionImage(const OpFunctionImage<FunctionMemberViewType>&) = delete;
    OpFunctionImage(OpFunctionImage<FunctionMemberViewType>&& other);
    ~OpFunctionImage() { this->stopTriggeringOnChildren(); }
    void addTrigger(const std::shared_ptr<FunctionMemberTriggerType>& trigger,
                    bool includeMembers) final;
    FunctionMemberViewType& view() final;
    const FunctionMemberViewType& view() const final;

    void evaluateImpl() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<FunctionMemberViewType> deepCopySelfForUnroll(
        const ExprRef<FunctionMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    ExprRef<FunctionMemberViewType>& getMember();
    const ExprRef<FunctionMemberViewType>& getMember() const;
    void reevaluate();
    bool optimise(PathExtension path) final;
};

#endif /* SRC_OPERATORS_OPFUNCTIONIMAGE_H_ */
