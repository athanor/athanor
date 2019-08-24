
#ifndef SRC_OPERATORS_OPFlattenOneLevel_H_
#define SRC_OPERATORS_OPFlattenOneLevel_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/sequence.h"

template <typename SequenceInnerType>
struct OpFlattenOneLevel : public SequenceView {
    struct OperandTrigger;
    struct InnerSequenceTrigger;
    ExprRef<SequenceView> operand;
    std::vector<UInt> startingIndices;
    std::shared_ptr<OperandTrigger> operandTrigger;
    std::vector<std::shared_ptr<InnerSequenceTrigger>> innerSequenceTriggers;

    OpFlattenOneLevel(ExprRef<SequenceView> operand)
        : operand(std::move(operand)) {
        members.emplace<ExprRefVec<SequenceInnerType>>();
    }

    void reevaluate();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    ExprRef<SequenceView> deepCopyForUnrollImpl(
        const ExprRef<SequenceView>& self,
        const AnyIterRef& iterator) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;

    std::ostream& dumpState(std::ostream& os) const final;
    std::pair<bool, ExprRef<SequenceView>> optimiseImpl(
        ExprRef<SequenceView>&, PathExtension path) final;
    void reattachAllInnerSequenceTriggers(bool deleteFirst);
    void assertValidStartingIndices() const;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPFlattenOneLevel_H_ */
