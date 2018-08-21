
#ifndef SRC_OPERATORS_OPTUPLEINDEX_H_
#define SRC_OPERATORS_OPTUPLEINDEX_H_

#include "base/base.h"
#include "types/int.h"
#include "types/tuple.h"
template <typename TupleMemberViewType>
struct OpTupleIndex : public ExprInterface<TupleMemberViewType> {
    struct TupleOperandTrigger;
    typedef typename AssociatedTriggerType<TupleMemberViewType>::type
        TupleMemberTriggerType;
    std::vector<std::shared_ptr<TriggerBase>> triggers;
    ExprRef<TupleView> tupleOperand;
    UInt index;
    bool defined = false;
    std::shared_ptr<TupleOperandTrigger> tupleTrigger;
    OpTupleIndex(ExprRef<TupleView> tupleOperand, UInt index)
        : tupleOperand(std::move(tupleOperand)), index(index) {}
    OpTupleIndex(const OpTupleIndex<TupleMemberViewType>&) = delete;
    OpTupleIndex(OpTupleIndex<TupleMemberViewType>&& other);
    ~OpTupleIndex() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(const std::shared_ptr<TupleMemberTriggerType>& trigger,
                        bool includeMembers, Int memberIndex) final;
    TupleMemberViewType& view() final;
    const TupleMemberViewType& view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<TupleMemberViewType> deepCopySelfForUnrollImpl(
        const ExprRef<TupleMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    std::pair<bool, ExprRef<TupleMemberViewType>> optimise(
        PathExtension path) final;
    ExprRef<TupleMemberViewType>& getMember();
    const ExprRef<TupleMemberViewType>& getMember() const;
    void reevaluateDefined();
};

#endif /* SRC_OPERATORS_OPTUPLEINDEX_H_ */
