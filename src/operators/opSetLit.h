#ifndef SRC_OPERATORS_OPSETLIT_H_
#define SRC_OPERATORS_OPSETLIT_H_
#include "base/base.h"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"

struct OpSetLit : public SetView {
    struct ExprTriggerBase {
        OpSetLit* op;
        UInt index;
        ExprTriggerBase(OpSetLit* op, UInt index) : op(op), index(index) {}
    };
    AnyExprVec operands;
    std::unordered_map<HashType, FastIterableIntSet> hashIndicesMap;
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;
    UInt numberUndefined = 0;
    OpSetLit(AnyExprVec operands) : operands(std::move(operands)) {}
    OpSetLit(const OpSetLit&) = delete;
    OpSetLit(OpSetLit&& other);
    ~OpSetLit() { this->stopTriggeringOnChildren(); }

    void addHash(HashType hash, size_t index) {
        hashIndicesMap[hash].insert(index);
    }

    std::pair<bool, size_t> removeHash(HashType hash, size_t index) {
        auto iter = hashIndicesMap.find(hash);
        debug_code(assert(iter != hashIndicesMap.end()));
        auto& indices = iter->second;
        if (indices.size() == 1) {
            hashIndicesMap.erase(iter);
            return std::make_pair(false, 0);
        } else {
            indices.erase(index);
            return std::make_pair(true, *indices.begin());
        }
    }
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer&) final;
    ExprRef<SetView> deepCopySelfForUnrollImpl(
        const ExprRef<SetView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined() final;
    bool optimise(PathExtension path) final;
};

#endif /* SRC_OPERATORS_OPSETLIT_H_ */
