#ifndef SRC_OPERATORS_OPSETLIT_H_
#define SRC_OPERATORS_OPSETLIT_H_
#include "base/base.h"
#include "operators/previousValueCache.h"
#include "operators/simpleOperator.hpp"
#include "types/set.h"
#include "utils/fastIterableIntSet.h"

template <typename OperandView>
struct OpSetLit;

template <>
struct OperatorTrates<OpSetLit<FunctionView>> {
    struct OperandTrigger;
};

template <typename OperandView>
struct OpSetLit
    : public SimpleUnaryOperator<SetView, OperandView, OpSetLit<OperandView>> {
    using SimpleUnaryOperator<SetView, OperandView,
                              OpSetLit<OperandView>>::SimpleUnaryOperator;
    /*class for storing information on each value (hash) h.*/
    struct OperandGroup {
        bool active = true;  // This value is being used in the set.
        // only false mid triggering, once stablised, this should always be
        // true.
        UInt focusOperand;  // index of operand that has hash h and is being
                            // used in the set view
        UInt focusOperandSetIndex;  // index of the operand as found in the set
                                    // view members
        HashSet<UInt> operands;     // all operands with hash h
    };
    HashMap<HashType, OperandGroup> hashIndicesMap;
    PreviousValueCache<HashType>
        cachedOperandHashes;  // map from operand index to hash.

    void reevaluateImpl(OperandView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpSetLit<OperandView>& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    void assertValidHashes();

    template <typename InnerViewType>
    void removeMemberFromSet(const HashType& hash,
                             OpSetLit<OperandView>::OperandGroup& operandGroup);
    template <typename InnerViewType>
    void addReplacementToSet(ExprRefVec<InnerViewType>& operands,
                             OperandGroup& og);

    template <typename InnerViewType>
    bool hashNotChanged(ExprRefVec<InnerViewType>& operands, UInt index);
    template <typename InnerViewType>
    void removeOperandValue(ExprRefVec<InnerViewType>& operands,
                            size_t index);
    template <typename InnerViewType>
    void addOperandValue(ExprRefVec<InnerViewType>& operands, size_t index,
                         bool insert = false);

    AnyExprVec& getChildrenOperands() final;
};
#endif /* SRC_OPERATORS_OPSETLIT_H_ */
