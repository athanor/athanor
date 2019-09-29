
#ifndef SRC_OPERATORS_OPSETINDEXINTERNAL_H_
#define SRC_OPERATORS_OPSETINDEXINTERNAL_H_

#include "base/base.h"
#include "types/int.h"
#include "types/set.h"
template <typename SetMemberViewType>
struct OpSetIndexInternal : public ExprInterface<SetMemberViewType>,
                            public TriggerContainer<SetMemberViewType> {
    struct SetOperandTrigger;
    typedef typename AssociatedTriggerType<SetMemberViewType>::type
        SetMemberTriggerType;
    struct MemberTrigger;

    ExprRef<SetView> setOperand;
    UInt indexOperand;
    std::shared_ptr<SetOperandTrigger> setOperandTrigger;
    std::shared_ptr<SetOperandTrigger> setMemberTrigger;
    std::shared_ptr<MemberTrigger> memberTrigger;
    bool exprDefined = false;
    bool isLastElementInSet;
    std::vector<size_t> elementOrder;
    OpSetIndexInternal(ExprRef<SetView> setOperand, UInt indexOperand)
        : setOperand(std::move(setOperand)),
          indexOperand(std::move(indexOperand)) {}
    OpSetIndexInternal(const OpSetIndexInternal<SetMemberViewType>&) = delete;
    OpSetIndexInternal(OpSetIndexInternal<SetMemberViewType>&&) = delete;
    ~OpSetIndexInternal() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(const std::shared_ptr<SetMemberTriggerType>& trigger,
                        bool includeMembers, Int memberIndex) final;
    OptionalRef<SetMemberViewType> view() final;
    OptionalRef<const SetMemberViewType> view() const final;
    bool allowForwardingOfTrigger();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SetMemberViewType> deepCopyForUnrollImpl(
        const ExprRef<SetMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;

    OptionalRef<ExprRef<SetMemberViewType>> getMember();
    OptionalRef<const ExprRef<SetMemberViewType>> getMember() const;
    void reevaluate();
    std::pair<bool, ExprRef<SetMemberViewType>> optimiseImpl(
        ExprRef<SetMemberViewType>&, PathExtension path) final;

    void reattachSetMemberTrigger();

    bool eventForwardedAsDefinednessChange();
    template <typename View = SetMemberViewType,
              typename std::enable_if<std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {}
    template <typename View = SetMemberViewType,
              typename std::enable_if<!std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool set) {
        Undefinable<View>::setAppearsDefined(set);
    }

    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPSETINDEXINTERNAL_H_ */
