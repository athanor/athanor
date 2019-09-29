#ifndef SRC_OPERATORS_OPMSETLIT_H_
#define SRC_OPERATORS_OPMSETLIT_H_
#include "base/base.h"
#include "types/mSet.h"

struct OpMSetLit : public MSetView {
    struct ExprTriggerBase {
        OpMSetLit* op;
        UInt index;
        ExprTriggerBase(OpMSetLit* op, UInt index) : op(op), index(index) {}
    };
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;

    OpMSetLit(AnyExprVec members) : MSetView(std::move(members)) {}
    OpMSetLit(const OpMSetLit&) = delete;
    OpMSetLit(OpMSetLit&&) = delete;
    ~OpMSetLit() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<MSetView> deepCopyForUnrollImpl(
        const ExprRef<MSetView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<MSetView>> optimiseImpl(ExprRef<MSetView>&,
                                                    PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPMSETLIT_H_ */
