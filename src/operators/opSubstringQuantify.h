
#ifndef SRC_OPERATORS_OPSUBSTRINGQUANTIFY_H_
#define SRC_OPERATORS_OPSUBSTRINGQUANTIFY_H_
#include "base/base.h"
#include "triggers/allTriggers.h"
#include "types/int.h"
#include "types/sequence.h"
template <typename SequenceMemberViewType>
struct OpSubstringQuantify : public SequenceView {
    struct SequenceOperandTrigger;
    struct BoundsTrigger;

    ExprRef<SequenceView> sequenceOperand;
    ExprRef<IntView> lowerBoundOperand;
    ExprRef<IntView> upperBoundOperand;
    const Int windowSize;
    // window points backwards
    Int cachedLowerBound;
    Int cachedUpperBound;
    std::shared_ptr<SequenceTrigger> sequenceOperandTrigger;
    std::shared_ptr<BoundsTrigger> lowerBoundTrigger;
    std::shared_ptr<BoundsTrigger> upperBoundTrigger;

    OpSubstringQuantify(ExprRef<SequenceView> sequenceOperand,
                        ExprRef<IntView> lowerBoundOperand,
                        ExprRef<IntView> upperBoundOperand, size_t windowSize)
        : sequenceOperand(std::move(sequenceOperand)),
          lowerBoundOperand(std::move(lowerBoundOperand)),
          upperBoundOperand(std::move(upperBoundOperand)),
          windowSize(windowSize) {}

    OpSubstringQuantify(const OpSubstringQuantify<SequenceMemberViewType>&) =
        delete;

    OpSubstringQuantify(OpSubstringQuantify<SequenceMemberViewType>&& other) =
        delete;
    ~OpSubstringQuantify() { this->stopTriggeringOnChildren(); }

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SequenceView> deepCopyForUnrollImpl(
        const ExprRef<SequenceView>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;

    void reevaluate();
    std::pair<bool, ExprRef<SequenceView>> optimiseImpl(
        ExprRef<SequenceView>&, PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const;
};

#endif /* SRC_OPERATORS_OPSUBSTRINGQUANTIFY_H_ */
