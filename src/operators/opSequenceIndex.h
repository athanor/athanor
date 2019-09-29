
#ifndef SRC_OPERATORS_OPSEQUENCEINDEX_H_
#define SRC_OPERATORS_OPSEQUENCEINDEX_H_
#include "base/base.h"
#include "triggers/allTriggers.h"
#include "types/int.h"
#include "types/sequence.h"
template <typename SequenceMemberViewType>
struct OpSequenceIndex : public ExprInterface<SequenceMemberViewType>,
                         public TriggerContainer<SequenceMemberViewType> {
    struct IndexTrigger;
    struct SequenceOperandTrigger;
    struct MemberTrigger;
    typedef typename AssociatedTriggerType<SequenceMemberViewType>::type
        SequenceMemberTriggerType;
    ExprRef<SequenceView> sequenceOperand;
    ExprRef<IntView> indexOperand;
    Int cachedIndex;
    bool locallyDefined = false;
    std::shared_ptr<SequenceOperandTrigger> sequenceOperandTrigger;
    std::shared_ptr<SequenceOperandTrigger> sequenceMemberTrigger;
    std::shared_ptr<MemberTrigger> memberTrigger;
    std::shared_ptr<IndexTrigger> indexTrigger;

    OpSequenceIndex(ExprRef<SequenceView> sequenceOperand,
                    ExprRef<IntView> indexOperand)
        : sequenceOperand(std::move(sequenceOperand)),
          indexOperand(std::move(indexOperand)) {}
    OpSequenceIndex(const OpSequenceIndex<SequenceMemberViewType>&) = delete;
    OpSequenceIndex(OpSequenceIndex<SequenceMemberViewType>&& other) = delete;
    ~OpSequenceIndex() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(
        const std::shared_ptr<SequenceMemberTriggerType>& trigger,
        bool includeMembers, Int memberIndex) final;
    OptionalRef<SequenceMemberViewType> view() final;
    OptionalRef<const SequenceMemberViewType> view() const final;

    bool allowForwardingOfTrigger();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SequenceMemberViewType> deepCopyForUnrollImpl(
        const ExprRef<SequenceMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;

    OptionalRef<ExprRef<SequenceMemberViewType>> getMember();
    OptionalRef<const ExprRef<SequenceMemberViewType>> getMember() const;
    void reevaluate();
    std::pair<bool, ExprRef<SequenceMemberViewType>> optimiseImpl(
        ExprRef<SequenceMemberViewType>&, PathExtension path) final;

    void reattachSequenceMemberTrigger();
    bool eventForwardedAsDefinednessChange();
    template <typename View = SequenceMemberViewType,
              typename std::enable_if<std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {}
    template <typename View = SequenceMemberViewType,
              typename std::enable_if<!std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool set) {
        Undefinable<View>::setAppearsDefined(set);
    }

    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPSEQUENCEINDEX_H_ */
