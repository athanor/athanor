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
    OpSequenceLit(OpSequenceLit&& other);
    ~OpSequenceLit() { this->stopTriggeringOnChildren(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<SequenceView> deepCopySelfForUnroll(
        const ExprRef<SequenceView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

#endif /* SRC_OPERATORS_OPSEQUENCELIT_H_ */
