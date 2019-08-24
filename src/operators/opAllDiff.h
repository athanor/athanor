
#ifndef SRC_OPERATORS_OPALLDIFF_H_
#define SRC_OPERATORS_OPALLDIFF_H_
#include <vector>
#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"
struct OpAllDiff;
template <>
struct OperatorTrates<OpAllDiff> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

struct OpAllDiff
    : public SimpleUnaryOperator<BoolView, SequenceView, OpAllDiff> {
    struct Count {
        size_t value = 0;
    };

    using SimpleUnaryOperator<BoolView, SequenceView,
                              OpAllDiff>::SimpleUnaryOperator;
    HashMap<HashType, FastIterableIntSet> hashIndicesMap;
    std::vector<HashType> indicesHashMap;
    FastIterableIntSet violatingOperands;

    OpAllDiff(OpAllDiff&&) = delete;
    OpAllDiff(const OpAllDiff&) = delete;
    size_t addHash(HashType hash, size_t memberIndex) {
        auto& indices = hashIndicesMap[hash];
        if (indices.size() == 1) {
            violatingOperands.insert(*indices.begin());
        }

        bool inserted = indices.insert(memberIndex).second;
        static_cast<void>(inserted);
        debug_code(assert(inserted));
        indicesHashMap[memberIndex] = hash;
        if (indices.size() > 1) {
            violatingOperands.insert(memberIndex);
        }
        return indices.size();
    }

    size_t removeHash(HashType hash, size_t memberIndex) {
        auto& indices = hashIndicesMap[hash];
        bool deleted = indices.erase(memberIndex);
        violatingOperands.erase(memberIndex);
        if (indices.size() == 1) {
            violatingOperands.erase(*indices.begin());
        }
        static_cast<void>(deleted);
        debug_code(assert(deleted));
        if (indices.size() == 0) {
            hashIndicesMap.erase(hash);
            return 0;
        } else {
            return indices.size();
        }
    }
    void reevaluateImpl(SequenceView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpAllDiff& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    
    void assertValidState();
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPALLDIFF_H_ */
