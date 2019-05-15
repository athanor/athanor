
#ifndef SRC_OPERATORS_OPFUNCTIONIMAGE_H_
#define SRC_OPERATORS_OPFUNCTIONIMAGE_H_

#include "base/base.h"
#include "types/function.h"
#include "types/int.h"
template <typename FunctionMemberViewType>
struct OpFunctionImage : public ExprInterface<FunctionMemberViewType>,
                         public TriggerContainer<FunctionMemberViewType> {
    typedef typename AssociatedTriggerType<FunctionMemberViewType>::type
        FunctionMemberTriggerType;
    struct FunctionOperandTrigger;
    struct PreImageTriggerBase {
        OpFunctionImage<FunctionMemberViewType>* op;
        PreImageTriggerBase(OpFunctionImage<FunctionMemberViewType>* op)
            : op(op) {}
        virtual ~PreImageTriggerBase() {}
    };
    struct MemberTrigger;
    ExprRef<FunctionView> functionOperand;
    AnyExprRef preImageOperand;
    Int cachedIndex;
    bool locallyDefined = false;
    std::shared_ptr<FunctionOperandTrigger> functionOperandTrigger;
    std::shared_ptr<FunctionOperandTrigger> functionMemberTrigger;
    std::shared_ptr<PreImageTriggerBase> preImageTrigger;
    std::shared_ptr<MemberTrigger> memberTrigger;

    OpFunctionImage(ExprRef<FunctionView> functionOperand,
                    AnyExprRef preImageOperand)
        : functionOperand(std::move(functionOperand)),
          preImageOperand(std::move(preImageOperand)) {}
    OpFunctionImage(const OpFunctionImage<FunctionMemberViewType>&) = delete;
    OpFunctionImage(OpFunctionImage<FunctionMemberViewType>&&) = delete;
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
    ExprRef<FunctionMemberViewType> deepCopyForUnrollImpl(
        const ExprRef<FunctionMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;

    OptionalRef<ExprRef<FunctionMemberViewType>> getMember();
    OptionalRef<const ExprRef<FunctionMemberViewType>> getMember() const;
    void reevaluate(bool recalculateCachedIndex = true);
    bool allowForwardingOfTrigger();
    lib::optional<UInt> calculateIndex() const;
    std::pair<bool, ExprRef<FunctionMemberViewType>> optimise(
        PathExtension path) final;

    void reattachFunctionMemberTrigger();
    bool eventForwardedAsDefinednessChange(bool recalculateIndex);
    template <typename View = FunctionMemberViewType,
              typename std::enable_if<std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {}
    template <typename View = FunctionMemberViewType,
              typename std::enable_if<!std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool set) {
        Undefinable<View>::setAppearsDefined(set);
    }
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPFUNCTIONIMAGE_H_ */
