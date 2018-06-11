
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
    Int cachedIndex;
    bool defined = false;
    bool locallyDefined = false;
    std::shared_ptr<SequenceOperandTrigger> sequenceTrigger;
    std::shared_ptr<SequenceOperandTrigger> sequenceMemberTrigger;
    std::shared_ptr<IndexTrigger> indexTrigger;
    OpSequenceIndex(ExprRef<SequenceView> sequenceOperand,
                    ExprRef<IntView> indexOperand)
        : sequenceOperand(std::move(sequenceOperand)),
          indexOperand(std::move(indexOperand)) {}
    OpSequenceIndex(const OpSequenceIndex<SequenceMemberViewType>&) = delete;
    OpSequenceIndex(OpSequenceIndex<SequenceMemberViewType>&& other);
    ~OpSequenceIndex() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(const std::shared_ptr<SequenceMemberTriggerType>& trigger,
                    bool includeMembers, Int memberIndex) final;
    SequenceMemberViewType& view() final;
    const SequenceMemberViewType& view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer&) final;
    ExprRef<SequenceMemberViewType> deepCopySelfForUnrollImpl(
        const ExprRef<SequenceMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    ExprRef<SequenceMemberViewType>& getMember();
    const ExprRef<SequenceMemberViewType>& getMember() const;
    void reevaluateDefined();
    bool optimise(PathExtension path) final;

    void reattachSequenceMemberTrigger(bool deleteFirst = true);
};

#endif /* SRC_OPERATORS_OPSEQUENCEINDEX_H_ */
