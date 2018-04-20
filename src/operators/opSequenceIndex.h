
#ifndef SRC_OPERATORS_OPSEQUENCEINDEX_H_
#define SRC_OPERATORS_OPSEQUENCEINDEX_H_

#include "base/base.h"
#include "types/int.h"
#include "types/sequence.h"
template <typename SequenceMemberViewType>
struct OpSequenceIndex : public ExprInterface<SequenceMemberViewType> {
    struct SequenceOperandTrigger;
    struct IndexTrigger;
    typedef typename AssociatedTriggerType<SequenceMemberViewType>::type
        SequenceMemberTriggerType;
    std::vector<std::shared_ptr<TriggerBase>> triggers;
    ExprRef<SequenceView> sequenceOperand;
    ExprRef<IntView> indexOperand;
    UInt cachedIndex;
    std::shared_ptr<SequenceOperandTrigger> sequenceTrigger;
    std::shared_ptr<IndexTrigger> indexTrigger;
    OpSequenceIndex(ExprRef<SequenceView> sequenceOperand,
                    ExprRef<IntView> indexOperand)
        : sequenceOperand(std::move(sequenceOperand)),
          indexOperand(std::move(indexOperand)) {}
    OpSequenceIndex(const OpSequenceIndex<SequenceMemberViewType>&) = delete;
    OpSequenceIndex(OpSequenceIndex<SequenceMemberViewType>&& other);
    ~OpSequenceIndex() { this->stopTriggeringOnChildren(); }
    void addTrigger(
        const std::shared_ptr<SequenceMemberTriggerType>& trigger) final;
    SequenceMemberViewType& view() final;
    const SequenceMemberViewType& view() const final;

    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<SequenceMemberViewType> deepCopySelfForUnroll(
        const ExprRef<SequenceMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

#endif /* SRC_OPERATORS_OPSEQUENCEINDEX_H_ */
