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

    template <typename OperandView>
    void swapHashes(size_t index1, size_t index2) {
        if (index1 == index2) {
            return;
        }
        auto& operands = mpark::get<ExprRefVec<OperandView>>(this->operands);
        bool operand1Defined = !operands[index1]->isUndefined();
        bool operand2Defined = !operands[index2]->isUndefined();
        HashType operand1Hash, operand2Hash;
        if (operand1Defined) {
            operand1Hash = getValueHash(operands[index1]->view());
            removeHash(operand1Hash, index1);
        }
        if (operand2Defined) {
            operand2Hash = getValueHash(operands[index2]->view());
            removeHash(operand2Hash, index2);
        }
        if (operand1Defined) {
            addHash(operand1Hash, index2);
        }
        if (operand2Defined) {
            addHash(operand2Hash, index1);
        }
    }
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
    bool isUndefined() final;
    std::pair<bool, ExprRef<SetView>> optimise(PathExtension path) final;
    void assertValidHashes();
};

#endif /* SRC_OPERATORS_OPSETLIT_H_ */
