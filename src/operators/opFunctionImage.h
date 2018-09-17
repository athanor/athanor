
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
          preImageOperand(std::move(preImageOperand)) {
        if (std::is_same<BoolView, FunctionMemberViewType>::value) {
            std::cerr << "I've temperarily disabled functions to booleans as "
                         "I'm not correctly handling relational semantics "
                         "forthe case where the function pre image becomes "
                         "undefined.\n";
            abort();
        }
    }
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

    OptionalRef<ExprRef<FunctionMemberViewType>> getMember();
    OptionalRef<const ExprRef<FunctionMemberViewType>> getMember() const;
    void reevaluate(bool recalculateCachedIndex = true);
    lib::optional<UInt> calculateIndex();
    std::pair<bool, ExprRef<FunctionMemberViewType>> optimise(
        PathExtension path) final;

    void reattachFunctionMemberTrigger(bool deleteFirst);
    bool eventForwardedAsDefinednessChange(bool recalculateIndex);
    template <typename View = FunctionMemberViewType,
              typename std::enable_if<std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {
        std::cerr << "Not handling function to bools where a function member "
                     "becomes undefined.\n";
        todoImpl();
    }
    template <typename View = FunctionMemberViewType,
              typename std::enable_if<!std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool set) {
        Undefinable<View>::setAppearsDefined(set);
    }
};

#endif /* SRC_OPERATORS_OPFUNCTIONIMAGE_H_ */
