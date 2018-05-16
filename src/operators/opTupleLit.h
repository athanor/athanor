#ifndef SRC_OPERATORS_OPTUPLELIT_H_
#define SRC_OPERATORS_OPTUPLELIT_H_
#include "base/base.h"
#include "types/tuple.h"

struct OpTupleLit : public TupleView {
    struct ExprTriggerBase {
        OpTupleLit* op;
        UInt index;
        ExprTriggerBase(OpTupleLit* op, UInt index) : op(op), index(index) {}
        virtual ~ExprTriggerBase() {}
    };
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;

    OpTupleLit(std::vector<AnyExprRef> members)
        : TupleView(std::move(members)) {}
    OpTupleLit(const OpTupleLit&) = delete;
    OpTupleLit(OpTupleLit&& other);
    ~OpTupleLit() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<TupleView> deepCopySelfForUnroll(
        const ExprRef<TupleView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined() final;
};

#endif /* SRC_OPERATORS_OPTUPLELIT_H_ */
