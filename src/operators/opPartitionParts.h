#ifndef SRC_OPERATORS_OPPARTITIONPARTS_H_
#define SRC_OPERATORS_OPPARTITIONPARTS_H_
#include "base/base.h"
#include "operators/simpleOperator.hpp"
#include "types/partition.h"
#include "types/setVal.h"

struct OpPartitionParts;

template <>
struct OperatorTrates<OpPartitionParts> {
    struct OperandTrigger;
};

struct OpPartitionParts
    : public SimpleUnaryOperator<SetView, PartitionView, OpPartitionParts> {
    std::vector<Int> partSetMap;
    std::vector<UInt> setPartMap;
    using SimpleUnaryOperator<SetView, PartitionView,
                              OpPartitionParts>::SimpleUnaryOperator;

    void reevaluateImpl(PartitionView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpPartitionParts& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

struct Part : public SetView {
    Part() { this->setAppearsDefined(true); }
    Part(const Part&) = delete;
    void evaluateImpl() final {}
    void startTriggeringImpl() final {}
    void stopTriggering() final {}
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final {
        mpark::visit(
            [&](auto& members) {
                for (auto& member : members) {
                    member->updateVarViolations(vioContext, vioContainer);
                }
            },
            this->members);
    }
    ExprRef<SetView> deepCopyForUnrollImpl(const ExprRef<SetView>&,
                                           const AnyIterRef&) const final {
        shouldNotBeCalledPanic;
    }

    std::ostream& dumpState(std::ostream& os) const final {
        return os << "Part: " << this->getViewIfDefined() << std::endl;
    }
    void findAndReplaceSelf(const FindAndReplaceFunction&,
                            PathExtension) final {
        shouldNotBeCalledPanic;
    }
    std::pair<bool, ExprRef<SetView>> optimiseImpl(ExprRef<SetView>& self,
                                                   PathExtension) final {
        return std::make_pair(false, self);
    }
    void debugSanityCheckImpl() const final {}
    std::string getOpName() const final { shouldNotBeCalledPanic; }
};
#endif /* SRC_OPERATORS_OPPARTITIONPARTS_H_ */
