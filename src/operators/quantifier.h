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
struct OptionalIndices {
    void copyOptionalIndices(OptionalIndices<ContainerType>&) const {}
};
template <>
struct OptionalIndices<SequenceView> {
    std::vector<size_t> indicesOfValuesToUnroll;
    void copyOptionalIndices(OptionalIndices<SequenceView>& o) const {
        o.indicesOfValuesToUnroll = indicesOfValuesToUnroll;
    }
};

template <typename ContainerType>
struct Quantifier : public SequenceView, public OptionalIndices<ContainerType> {
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
    ExprRef<BoolView> condition = nullptr;
    bool containerDefined = true;
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType>> containerTrigger;
    std::shared_ptr<ContainerTrigger<ContainerType>> containerDelayedTrigger;
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;
    AnyExprVec valuesToUnroll;

    Quantifier(ExprRef<ContainerType> container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}
    Quantifier(Quantifier<ContainerType>&&) = delete;
    inline void setExpression(AnyExprRef exprIn) {
        expr = std::move(exprIn);
        mpark::visit(
            [&](auto& expr) { members.emplace<ExprRefVec<viewType(expr)>>(); },
            expr);
    }

    inline void setCondition(const ExprRef<BoolView>& condition) {
        this->condition = condition;
    }
    ~Quantifier() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SequenceView> deepCopyForUnrollImpl(
        const ExprRef<SequenceView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;

    template <typename T>
    inline IterRef<T> newIterRef() {
        return std::make_shared<Iterator<T>>(quantId, nullptr);
    }

    template <typename View>
    void setContainerInnerType() {
        valuesToUnroll.emplace<ExprRefVec<View>>();
    }
    bool triggering();
    template <typename ViewType>
    void unroll(UInt index, ExprRef<ViewType> newView);
    void roll(UInt index);
    void swapExprs(UInt index1, UInt index2);
    template <typename ViewType>
    void startTriggeringOnExpr(UInt index, ExprRef<ViewType>& expr);
    template <typename ViewType>
    void stopTriggeringOnExpr(UInt oldIndex);
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;

    std::pair<bool, ExprRef<SequenceView>> optimise(PathExtension path);
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
