#ifndef SRC_OPERATORS_OPSETLIT_H_
#define SRC_OPERATORS_OPSETLIT_H_
#include "base/base.h"
#include "operators/previousValueCache.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"

struct OpSetLit : public SetView {
    struct ExprTriggerBase {
        OpSetLit* op;
        UInt index;
        ExprTriggerBase(OpSetLit* op, UInt index) : op(op), index(index) {}
    };
    struct OperandGroup {
        UInt focusOperand;
        UInt focusOperandSetIndex;
        std::unordered_set<UInt> operands;
    };
    AnyExprVec operands;
    std::unordered_map<HashType, OperandGroup> hashIndicesMap;
    PreviousValueCache<HashType> cachedOperandHashes;
    PreviousValueCache<HashType> cachedSetHashes;

    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;
    UInt numberUndefined = 0;
    OpSetLit(AnyExprVec operands) : operands(std::move(operands)) {}
    OpSetLit(const OpSetLit&) = delete;
    OpSetLit(OpSetLit&& other);
    ~OpSetLit() { this->stopTriggeringOnChildren(); }

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SetView> deepCopySelfForUnrollImpl(
        const ExprRef<SetView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<SetView>> optimise(PathExtension path) final;
    void assertValidHashes();
    template <typename View>
    ExprRefVec<View>& getOperands() {
        return mpark::get<ExprRefVec<View>>(operands);
    }
    template <typename View>
    void removeValue(size_t index, HashType hash);
    template <typename View>
    void addValue(size_t index, bool insert = false);
};

#endif /* SRC_OPERATORS_OPSETLIT_H_ */
