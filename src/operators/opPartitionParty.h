
#ifndef SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_
#define SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_
#include "operators/simpleOperator.h"
#include "types/partition.h"
#include "types/set.h"
template <typename OperandView>
struct OpPartitionParty;
template <typename OperandView>
struct OperatorTrates<OpPartitionParty<OperandView>> {
    struct LeftTrigger;
    struct RightTrigger;
};

template <typename OperandView>
struct OpPartitionParty
    : public SimpleBinaryOperator<SetView, OperandView, PartitionView,
                                  OpPartitionParty<OperandView>> {
    lib::optional<UInt> cachedPart;
    lib::optional<UInt> cachedPartitionMemberIndex;
    using SimpleBinaryOperator<
        SetView, OperandView, PartitionView,
        OpPartitionParty<OperandView>>::SimpleBinaryOperator;

    void reevaluateImpl(OperandView& leftView, PartitionView& rightView, bool,
                        bool);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpPartitionParty& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_OPFUNCTIONPREIMAGE_H_ */
