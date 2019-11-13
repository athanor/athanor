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
    AnyExprVec operands;
    HashMap<HashType, OperandGroup> hashIndicesMap;
    PreviousValueCache<HashType>
        cachedOperandHashes;  // map from operand index to hash.
    PreviousValueCache<HashType>
        cachedSetHashes;  // map from set member index to hash.

    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;
    OpSetLit(AnyExprVec operands) : operands(std::move(operands)) {}
    OpSetLit(const OpSetLit&) = delete;
    OpSetLit(OpSetLit&&) = delete;
    ~OpSetLit() { this->stopTriggeringOnChildren(); }

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SetView> deepCopyForUnrollImpl(
        const ExprRef<SetView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<SetView>> optimiseImpl(ExprRef<SetView>&,
                                                   PathExtension path) final;
    void assertValidHashes();
    template <typename View>
    ExprRefVec<View>& getOperands() {
        return lib::get<ExprRefVec<View>>(operands);
    }
    template <typename InnerViewType>
    void removeMemberFromSet(const HashType& hash,
                             OpSetLit::OperandGroup& operandGroup);
    template <typename View>
    void addReplacementToSet(OperandGroup& og);

    template <typename InnerViewType>
    bool hashNotChanged(UInt index);
    template <typename View>
    void removeOperandValue(size_t index, HashType hash);
    template <typename View>
    void addOperandValue(size_t index, bool insert = false);
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    AnyExprVec& getChildrenOperands() final;
};

#endif /* SRC_OPERATORS_OPSETLIT_H_ */
