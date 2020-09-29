
#ifndef SRC_OPERATORS_OPTOGETHER_H_
#define SRC_OPERATORS_OPTOGETHER_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/bool.h"
#include "types/partition.h"
#include "types/set.h"
struct OpTogether;

template <>
struct OperatorTrates<OpTogether> {
    struct LeftTrigger;
    struct RightTrigger;
};

struct OpTogether : public SimpleBinaryOperator<BoolView, SetView,
                                                PartitionView, OpTogether> {
    std::vector<TriggerOwner<OperatorTrates<OpTogether>::RightTrigger>>
        partitionMemberTriggers;  // one trigger for each member of the set
                                  // operand, triggering on one specific member
                                  // of the partition. //may be null if set
                                  // member does not exist in partition.

    using SimpleBinaryOperator<BoolView, SetView, PartitionView,
                               OpTogether>::SimpleBinaryOperator;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void reevaluateImpl(SetView& leftView, PartitionView& rightView, bool,
                        bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpTogether& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPTOGETHER_H_ */
