#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
#include "base/base.h"
#include "operators/iterator.h"
#include "types/bool.h"
#include "types/sequence.h"

inline static UInt64 nextQuantId() {
    static UInt64 quantId = 0;
    return quantId++;
}

template <typename Container>
struct ContainerTrigger;

template <typename View, EnableIfView<View> = 0>
struct QueuedUnrollValue {
    bool directUnrollExpr = false;  // if true, when unrolling unrollExpr() will
    // be called, not unroll()
    UInt index;           // index of where to unroll this value
    ExprRef<View> value;  // value to unroll
    QueuedUnrollValue(bool directUnrollExpr, UInt index, ExprRef<View> value)
        : directUnrollExpr(directUnrollExpr), index(index), value(value) {}
};

template <typename ContainerType>
struct Quantifier : public SequenceView {
    // for each expr or condition that is unrolled, this class is used as the
    // base class for triggers on that expr/condition
    struct ExprTriggerBase : public virtual TriggerBase {
        Quantifier<ContainerType>* op;
        UInt index;
        ExprTriggerBase(Quantifier<ContainerType>* op, UInt index)
            : op(op), index(index) {}
        virtual ~ExprTriggerBase() {}
    };
    // for quantifiers with conditions
    struct UnrolledCondition {
        static const UInt UNASSIGNED_EXPR_INDEX =
            std::numeric_limits<UInt>::max();

        // the unrolled condition
        ExprRef<BoolView> condition;
        // the cached state of the condition
        bool cachedValue;
        // the index of the unrolled expr that this condition maps to
        // when this condition is false, index points to previous expr.  If ther
        // is no previous expr, it points to  UNASSIGNED_EXPR_INDEX
        UInt exprIndex;
        // the trigger watching this condition
        std::shared_ptr<ExprTriggerBase> trigger;
        UnrolledCondition(ExprRef<BoolView> condition, UInt exprIndex)
            : condition(std::move(condition)),
              cachedValue(this->condition->view()->violation == 0),
              exprIndex(exprIndex) {
            debug_code(assert(this->condition->isEvaluated()));
        }
        friend inline std::ostream& operator<<(std::ostream& os,
                                               const UnrolledCondition& cond) {
            os << "condition(cachedValue=" << cond.cachedValue
               << ",exprIndex=" << cond.exprIndex << ",condition=";
            return cond.condition->dumpState(os) << ")";
        }
    };

    const UInt64 quantId;
    ExprRef<ContainerType> container;
    AnyExprRef expr = ExprRef<BoolView>(nullptr);
    ExprRef<BoolView> condition = nullptr;
    std::vector<UnrolledCondition> unrolledConditions;
    bool containerDefined = true;
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType>> containerTrigger;
    std::vector<std::shared_ptr<ExprTriggerBase>> exprTriggers;
    bool optimisedToNotUpdateIndices =
        false;  // when quantifying over a sequence and the index of each
                // element is not used.
    Quantifier(ExprRef<ContainerType> container,
               const UInt64 id = nextQuantId())
        : quantId(id), container(std::move(container)) {}
    inline void setExpression(AnyExprRef exprIn) {
        expr = std::move(exprIn);
        lib::visit(
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
    bool isQuantifier() const final;
    template <typename T>
    inline IterRef<T> newIterRef() {
        return std::make_shared<Iterator<T>>(quantId, nullptr);
    }

    bool triggering();

    UInt numberUnrolled() const {
        // if quantifier has conditions, number unrolled is the number of
        // conditions unrolled.  Otherwise it is the number of actual exprs
        // unrolled.
        return unrolledIterVals.size();
    }

    template <typename View>
    void unroll(QueuedUnrollValue<View> queuedValue);

    template <typename View>
    UnrolledCondition& unrollCondition(UInt index, ExprRef<View> newView,
                                       IterRef<View> newIterRef);

    template <typename View>
    void unrollExpr(UInt index, ExprRef<View> newView, IterRef<View> iterRef);

    void roll(UInt index);
    UnrolledCondition rollCondition(UInt index);
    void rollExpr(UInt index);

    void notifyContainerMembersSwapped(UInt index1, UInt index2);

    template <typename View>
    void startTriggeringOnExpr(UInt index, ExprRef<View>& expr);

    void startTriggeringOnCondition(UInt index, bool fixUpOtherIndices);

    void stopTriggeringOnExpr(UInt oldIndex);
    void stopTriggeringOnCondition(UInt oldIndex, UnrolledCondition& condition);

    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;

    std::pair<bool, ExprRef<SequenceView>> optimiseImpl(
        ExprRef<SequenceView>& self, PathExtension path);
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
