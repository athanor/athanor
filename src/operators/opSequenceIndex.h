
#ifndef SRC_OPERATORS_OPSEQUENCEINDEX_H_
#define SRC_OPERATORS_OPSEQUENCEINDEX_H_

#include "base/base.h"
#include "types/int.h"
#include "types/sequence.h"
template <typename SequenceMemberViewType>
struct OpSequenceIndex : public ExprInterface<SequenceMemberViewType> {
    struct IndexTrigger;
    struct SequenceOperandTrigger;
    typedef typename AssociatedTriggerType<SequenceMemberViewType>::type
        SequenceMemberTriggerType;
    std::vector<std::shared_ptr<TriggerBase>> triggers;
    ExprRef<SequenceView> sequenceOperand;
    ExprRef<IntView> indexOperand;
    Int cachedIndex;
    bool locallyDefined = false;
    std::shared_ptr<SequenceOperandTrigger> sequenceOperandTrigger;
    std::shared_ptr<SequenceOperandTrigger> sequenceMemberTrigger;
    std::shared_ptr<IndexTrigger> indexTrigger;

    OpSequenceIndex(ExprRef<SequenceView> sequenceOperand,
                    ExprRef<IntView> indexOperand)
        : sequenceOperand(std::move(sequenceOperand)),
          indexOperand(std::move(indexOperand)) {
        if (std::is_same<BoolView, SequenceMemberViewType>::value) {
            std::cerr << "I've temperarily disabled OpSequenceIndex for "
                         "sequences of booleans as "
                         "I'm not correctly handling relational semantics "
                         "forthe case where the sequence indexbecomes "
                         "undefined.\n";
            abort();
        }
    }
    OpSequenceIndex(const OpSequenceIndex<SequenceMemberViewType>&) = delete;
    OpSequenceIndex(OpSequenceIndex<SequenceMemberViewType>&& other);
    ~OpSequenceIndex() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(
        const std::shared_ptr<SequenceMemberTriggerType>& trigger,
        bool includeMembers, Int memberIndex) final;
    OptionalRef<SequenceMemberViewType> view() final;
    OptionalRef<const SequenceMemberViewType> view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SequenceMemberViewType> deepCopySelfForUnrollImpl(
        const ExprRef<SequenceMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;

    OptionalRef<ExprRef<SequenceMemberViewType>> getMember();
    OptionalRef<const ExprRef<SequenceMemberViewType>> getMember() const;
    void reevaluate(bool recalculateCachedIndex = true);
    std::pair<bool, ExprRef<SequenceMemberViewType>> optimise(
        PathExtension path) final;

    void reattachSequenceMemberTrigger();
    bool eventForwardedAsDefinednessChange(bool recalculateIndex);
    template <typename View = SequenceMemberViewType,
              typename std::enable_if<std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {
        std::cerr << "Not handling sequence to bools where a sequence member "
                     "becomes undefined.\n";
        todoImpl();
    }
    template <typename View = SequenceMemberViewType,
              typename std::enable_if<!std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool set) {
        Undefinable<View>::setAppearsDefined(set);
    }
};

#endif /* SRC_OPERATORS_OPSEQUENCEINDEX_H_ */
