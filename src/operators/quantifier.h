#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
#include "base/base.h"
#include "operators/iterator.h"
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
        UInt index;
        ExprTriggerBase(Quantifier<ContainerType>* op, UInt index)
            : op(op), index(index) {}
        virtual ~ExprTriggerBase() {}
    };
    const int quantId;
    ExprRef<ContainerType> container;
    AnyExprRef expr = ExprRef<BoolView>(nullptr);
    bool containerDefined = true;
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType>> containerTrigger;
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;

    Quantifier(ExprRef<ContainerType> container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}
    Quantifier(Quantifier<ContainerType>&& other);
    inline void setExpression(AnyExprRef exprIn) {
        expr = std::move(exprIn);
        mpark::visit(
            [&](auto& expr) { members.emplace<ExprRefVec<viewType(expr)>>(); },
            expr);
    }

    ~Quantifier() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<SequenceView> deepCopySelfForUnroll(
        const ExprRef<SequenceView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;

    template <typename T>
    inline IterRef<T> newIterRef() {
        return std::make_shared<Iterator<T>>(quantId, nullptr);
    }

    bool triggering();
    template <typename ViewType>
    void unroll(UInt index, const ExprRef<ViewType>& newView);
    void roll(UInt index);
    void swap(UInt index1, UInt index2, bool notifyBeginEnd = true);
    template <typename ViewType>
    void startTriggeringOnExpr(UInt index, ExprRef<ViewType>& expr);
    template <typename ViewType>
    void stopTriggeringOnExpr(UInt oldIndex);
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    bool optimise();
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
