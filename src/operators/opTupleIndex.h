
#ifndef SRC_OPERATORS_OPTUPLEINDEX_H_
#define SRC_OPERATORS_OPTUPLEINDEX_H_

#include "base/base.h"
#include "types/int.h"
#include "types/tuple.h"
template <typename TupleMemberViewType>
struct OpTupleIndex : public ExprInterface<TupleMemberViewType>,
                      public TriggerContainer<TupleMemberViewType> {
    struct TupleOperandTrigger;
    typedef typename AssociatedTriggerType<TupleMemberViewType>::type
        TupleMemberTriggerType;
    struct MemberTrigger;

    ExprRef<TupleView> tupleOperand;
    UInt indexOperand;
    std::shared_ptr<TupleOperandTrigger> tupleOperandTrigger;
    std::shared_ptr<TupleOperandTrigger> tupleMemberTrigger;
    std::shared_ptr<MemberTrigger> memberTrigger;
    bool exprDefined = false;
    OpTupleIndex(ExprRef<TupleView> tupleOperand, UInt indexOperand)
        : tupleOperand(std::move(tupleOperand)),
          indexOperand(std::move(indexOperand)) {}
    OpTupleIndex(const OpTupleIndex<TupleMemberViewType>&) = delete;
    OpTupleIndex(OpTupleIndex<TupleMemberViewType>&&) = delete;
    ~OpTupleIndex() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(const std::shared_ptr<TupleMemberTriggerType>& trigger,
                        bool includeMembers, Int memberIndex) final;
    OptionalRef<TupleMemberViewType> view() final;
    OptionalRef<const TupleMemberViewType> view() const final;
    bool allowForwardingOfTrigger();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<TupleMemberViewType> deepCopyForUnrollImpl(
        const ExprRef<TupleMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;

    OptionalRef<ExprRef<TupleMemberViewType>> getMember();
    OptionalRef<const ExprRef<TupleMemberViewType>> getMember() const;
    void reevaluate();
    std::pair<bool, ExprRef<TupleMemberViewType>> optimiseImpl(
        ExprRef<TupleMemberViewType>&, PathExtension path) final;

    void reattachTupleMemberTrigger();

    bool eventForwardedAsDefinednessChange();
    template <typename View = TupleMemberViewType,
              typename std::enable_if<std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {}
    template <typename View = TupleMemberViewType,
              typename std::enable_if<!std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool set) {
        Undefinable<View>::setAppearsDefined(set);
    }

    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPTUPLEINDEX_H_ */
