
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
    UInt indexOperand;
    std::shared_ptr<TupleOperandTrigger> tupleOperandTrigger;
    std::shared_ptr<TupleOperandTrigger> tupleMemberTrigger;

    OpTupleIndex(ExprRef<TupleView> tupleOperand, UInt indexOperand)
        : tupleOperand(std::move(tupleOperand)),
          indexOperand(std::move(indexOperand)) {
        if (std::is_same<BoolView, TupleMemberViewType>::value) {
            std::cerr << "I've temperarily disabled OpTupleIndex for "
                         "tuples of booleans as "
                         "I'm not correctly handling relational semantics "
                         "forthe case where the tuple indexbecomes "
                         "undefined.\n";
            abort();
        }
    }
    OpTupleIndex(const OpTupleIndex<TupleMemberViewType>&) = delete;
    OpTupleIndex(OpTupleIndex<TupleMemberViewType>&& other);
    ~OpTupleIndex() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(const std::shared_ptr<TupleMemberTriggerType>& trigger,
                        bool includeMembers, Int memberIndex) final;
    OptionalRef<TupleMemberViewType> view() final;
    OptionalRef<const TupleMemberViewType> view() const final;

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

    OptionalRef<ExprRef<TupleMemberViewType>> getMember();
    OptionalRef<const ExprRef<TupleMemberViewType>> getMember() const;
    void reevaluate();
    std::pair<bool, ExprRef<TupleMemberViewType>> optimise(
        PathExtension path) final;

    void reattachTupleMemberTrigger();
    bool eventForwardedAsDefinednessChange();
    template <typename View = TupleMemberViewType,
              typename std::enable_if<std::is_same<BoolView, View>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {
        std::cerr << "Not handling tuple to bools where a tuple member "
                     "becomes undefined.\n";
        todoImpl();
    }
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
