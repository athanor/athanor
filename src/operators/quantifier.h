#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
#include "base/base.h"
#include "types/sequence.h"
inline static u_int64_t nextQuantId() {
    static u_int64_t quantId = 0;
    return quantId++;
}
template <typename Container>
struct ContainerTrigger;

template <typename ContainerType>
struct Quantifier : public SequenceView {
    struct ExprTriggerBase {
        Quantifier<ContainerType>* op;
        ExprTriggerBase(Quantifier<ContainerType>* op) : op(op) {}
    };
    const int quantId;
    ExprRef<ContainerType> container;
    AnyExprRef expr = ViewRef<BoolView>(nullptr);
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType>> containerTrigger;
    std::shared_ptr<ExprTriggerBase> exprTrigger;

    Quantifier(ExprRef<ContainerType> container, const int id = nextQuantId());
    Quantifier(const Quantifier<ContainerType>&& other);
    ~Quantifier() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<SequenceView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    void setExpression(AnyExprRef exprIn);

    bool triggering();
    template <typename ViewType>
    void unroll(const ExprRef<ViewType>& newView);
    void roll(UInt index);
    template <typename ViewType>
    void startTriggeringOnExpr(ExprRef<ViewType>& expr);
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
