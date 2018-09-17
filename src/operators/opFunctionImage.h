
#ifndef SRC_OPERATORS_OPFUNCTIONIMAGE_H_
#define SRC_OPERATORS_OPFUNCTIONIMAGE_H_

#include "base/base.h"
#include "types/function.h"
#include "types/int.h"
template <typename FunctionMemberViewType>
struct OpFunctionImage : public ExprInterface<FunctionMemberViewType> {
    struct FunctionOperandTrigger;
    typedef typename AssociatedTriggerType<FunctionMemberViewType>::type
        FunctionMemberTriggerType;
    struct PreImageTriggerBase {
        OpFunctionImage<FunctionMemberViewType>* op;
        PreImageTriggerBase(OpFunctionImage<FunctionMemberViewType>* op)
            : op(op) {}
        virtual ~PreImageTriggerBase() {}
    };

    std::vector<std::shared_ptr<TriggerBase>> triggers;
    ExprRef<FunctionView> functionOperand;
    AnyExprRef preImageOperand;
    Int cachedIndex;
    bool locallyDefined = false;
    std::shared_ptr<FunctionOperandTrigger> functionOperandTrigger;
    std::shared_ptr<FunctionOperandTrigger> functionMemberTrigger;
    std::shared_ptr<PreImageTriggerBase> preImageTrigger;

    OpFunctionImage(ExprRef<FunctionView> functionOperand,
                    AnyExprRef preImageOperand)
        : functionOperand(std::move(functionOperand)),
          preImageOperand(std::move(preImageOperand)) {}
    OpFunctionImage(const OpFunctionImage<FunctionMemberViewType>&) = delete;
    OpFunctionImage(OpFunctionImage<FunctionMemberViewType>&& other);
    ~OpFunctionImage() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(
        const std::shared_ptr<FunctionMemberTriggerType>& trigger,
        bool includeMembers, Int memberIndex) final;
    OptionalRef<FunctionMemberViewType> view() final;
    OptionalRef<const FunctionMemberViewType> view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<FunctionMemberViewType> deepCopySelfForUnrollImpl(
        const ExprRef<FunctionMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    OptionalRef<ExprRef<FunctionMemberViewType>> getMember();
    OptionalRef<const ExprRef<FunctionMemberViewType>> getMember() const;
    void reevaluate(bool recalculateCachedIndex = true);
    lib::optional<Int> calculateIndex();
    std::pair<bool, ExprRef<FunctionMemberViewType>> optimise(
        PathExtension path) final;

    void reattachFunctionMemberTrigger(bool deleteFirst);
    bool eventForwardedAsDefinednessChange(bool wasDefined,
                                           bool wasLocallyDefined);
    bool eventForwardedAsDefinednessChange(bool recalculateIndex);
};

#endif /* SRC_OPERATORS_OPFUNCTIONIMAGE_H_ */
