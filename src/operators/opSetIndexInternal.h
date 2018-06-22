
#ifndef SRC_OPERATORS_OPSETINDEXINTERNAL_H_
#define SRC_OPERATORS_OPSETINDEXINTERNAL_H_

#include "base/base.h"
#include "types/set.h"

template <typename SetMemberViewType>
struct OpSetIndexInternal : public ExprInterface<SetMemberViewType> {
    typedef typename AssociatedTriggerType<SetMemberViewType>::type
        SetMemberTriggerType;

    struct SortedSet {
        struct SetOperandTrigger;
        bool evaluated = false;
        ExprRef<SetView> setOperand;
        std::shared_ptr<SetOperandTrigger> setTrigger;
        std::vector<OpSetIndexInternal*> parents;
        std::vector<size_t> parentSetMapping;
        std::vector<size_t> setParentMapping;
        std::shared_ptr<SortedSet> otherSortedSet;
        size_t parentCopyCount = 0;

        SortedSet(ExprRef<SetView> setOperand)
            : setOperand(std::move(setOperand)) {}
        SortedSet(const SortedSet&) = delete;
        SortedSet(SortedSet&& other);
        void evaluate();
        void reevaluate();
        void notifySetMemberValueChange(UInt index);
        void swapMembers(size_t index1, size_t index2);
        ExprRef<SetMemberViewType>& getMember(size_t index);
        const ExprRef<SetMemberViewType>& getMember(size_t index) const;
    };
    std::shared_ptr<SortedSet> sortedSet;
    std::vector<std::shared_ptr<TriggerBase>> triggers;
    UInt index;
    bool defined = false;
    OpSetIndexInternal(std::shared_ptr<SortedSet> sortedSet, UInt index)
        : sortedSet(std::move(sortedSet)), index(index) {
        if (this->sortedSet->parents.size() <= this->index) {
            this->sortedSet->parents.resize(this->index + 1);
        }
        this->sortedSet->parents[this->index] = this;
    }
    OpSetIndexInternal(const OpSetIndexInternal<SetMemberViewType>&) = delete;
    OpSetIndexInternal(OpSetIndexInternal<SetMemberViewType>&& other);
    ~OpSetIndexInternal() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(const std::shared_ptr<SetMemberTriggerType>& trigger,
                        bool includeMembers, Int memberIndex) final;
    SetMemberViewType& view() final;
    const SetMemberViewType& view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer&) final;
    ExprRef<SetMemberViewType> deepCopySelfForUnrollImpl(
        const ExprRef<SetMemberViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    std::pair<bool, ExprRef<SetMemberViewType>> optimise(
        PathExtension path) final;
    ExprRef<SetMemberViewType>& getMember();
    const ExprRef<SetMemberViewType>& getMember() const;
    void reevaluateDefined();
    void notifyPossibleMemberSwap();
    void notifyMemberSwapped();
};

#endif /* SRC_OPERATORS_OPSETINDEXINTERNAL_H_ */
