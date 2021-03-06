#ifndef SRC_OPERATORS_OPSEQUENCELIT_H_
#define SRC_OPERATORS_OPSEQUENCELIT_H_
#include "base/base.h"
#include "types/sequence.h"

struct OpSequenceLit : public SequenceView {
    struct ExprTriggerBase {
        OpSequenceLit* op;
        UInt index;
        ExprTriggerBase(OpSequenceLit* op, UInt index) : op(op), index(index) {}
    };
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;

    OpSequenceLit(AnyExprVec members) : SequenceView(std::move(members)) {}
    OpSequenceLit(const OpSequenceLit&) = delete;
    OpSequenceLit(OpSequenceLit&&) = delete;
    ~OpSequenceLit() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SequenceView> deepCopyForUnrollImpl(
        const ExprRef<SequenceView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<SequenceView>> optimiseImpl(
        ExprRef<SequenceView>&, PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    AnyExprVec& getChildrenOperands() final;
    void hashChecksImpl() const final;
};

#endif /* SRC_OPERATORS_OPSEQUENCELIT_H_ */
