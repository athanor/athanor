
#ifndef SRC_OPERATORS_OPTOGETHER_H_
#define SRC_OPERATORS_OPTOGETHER_H_
#include "base/base.h"
#include "triggers/allTriggers.h"
#include "types/bool.h"
#include "types/partition.h"
template <typename PartitionMemberViewType>
struct OpTogether : public BoolView {
    struct PartitionOperandTrigger;
    template <bool isLeft>
    struct OperandTrigger;
    ExprRef<PartitionView> partitionOperand;
    ExprRef<PartitionMemberViewType> left;
    ExprRef<PartitionMemberViewType> right;
    UInt cachedLeftIndex;
    UInt cachedRightIndex;
    std::shared_ptr<PartitionOperandTrigger> partitionOperandTrigger;
    std::shared_ptr<PartitionOperandTrigger> leftMemberTrigger;
    std::shared_ptr<PartitionOperandTrigger> rightMemberTrigger;
    std::shared_ptr<OperandTrigger<true>> leftOperandTrigger;
    std::shared_ptr<OperandTrigger<false>> rightOperandTrigger;

    OpTogether(ExprRef<PartitionView> partitionOperand,
               ExprRef<PartitionMemberViewType> left,
               ExprRef<PartitionMemberViewType> right)
        : partitionOperand(std::move(partitionOperand)),
          left(std::move(left)),
          right(std::move(right)) {}

    OpTogether(const OpTogether<PartitionMemberViewType>&) = delete;
    OpTogether(OpTogether<PartitionMemberViewType>&&) = delete;
    ~OpTogether() { this->stopTriggeringOnChildren(); }

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<BoolView> deepCopyForUnrollImpl(
        const ExprRef<BoolView>& self, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;

    void reevaluate(bool recalculateCachedLeftIndex,
                    bool recalculateCachedRightIndex);
    void reattachPartitionMemberTrigger(bool leftTrigger, bool rightTrigger);
    std::pair<bool, ExprRef<BoolView>> optimiseImpl(ExprRef<BoolView>&,
                                                    PathExtension path) final;

    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPTOGETHER_H_ */
