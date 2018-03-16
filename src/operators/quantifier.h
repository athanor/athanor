#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
#include "base/base.h"
#include "operators/quantifierView.h"
#include "types/allTypes.h"
template <typename Container>
struct InitialUnroller;

template <>
struct InitialUnroller<SetView>;
template <>
struct InitialUnroller<MSetView>;
template <typename Container, typename ExprType>
struct ContainerTrigger;
template <typename ExprType>
struct ContainerTrigger<SetView, ExprType>;
template <typename ExprType>
struct ContainerTrigger<MSetView, ExprType>;

inline static u_int64_t nextQuantId() {
    static u_int64_t quantId = 0;
    return quantId++;
}

template <typename ContainerType, typename ExprType>
struct Quantifier : public QuantifierView<ExprType> {
    using QuantifierView<ExprType>::exprs;
    using QuantifierView<ExprType>::triggers;

    const int quantId;
    ExprRef<ContainerType> container;
    ExprRef<ExprType> expr = ViewRef<ExprType>(nullptr);
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType, ExprType>> containerTrigger;

    Quantifier(ExprRef<ContainerType> container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}
    ~Quantifier() { stopTriggeringOnContainer(); }
    Quantifier(const Quantifier<ContainerType, ExprRef<ExprType>>&& other)
        : QuantifierView<ExprRef<ExprType>>(std::move(other)),
          quantId(other.quantId),
          container(std::move(other.container)),
          expr(std::move(other.expr)),
          unrolledIterVals(std::move(other.unrolledIterVals)),
          containerTrigger(std::move(other.containerTrigger)) {
        setTriggerParent(this, containerTrigger);
    }

    inline void setExpression(ExprRef<ExprType> exprIn) {
        expr = std::move(exprIn);
    }

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    inline bool triggering() { return static_cast<bool>(containerTrigger); }
    template <typename ViewType>
    inline void unroll(const ViewRef<ViewType>& newView) {
        debug_log("unrolling " << newView);
        auto iterRef = this->newIterRef<ViewType>();
        // choose which expr to copy from
        // use previously unrolled expr if available as  can be most
        // efficiently updated to new unrolled value  otherwise use expr
        // template
        auto& exprToCopy = (exprs.empty()) ? expr : exprs.back();
        // decide if expr that is going to be unrolled needs evaluating.
        // It needs evaluating if we are in triggering() mode and if the
        // expr being unrolled is taken from the expr template as the
        // expr will not have been evaluated before.  If we are instead
        // copying from an already unrolled expr, it  need not be
        // evaluated, simply copy it and trigger the change in value.
        bool evaluateExpr = exprs.empty() && triggering();
        const auto& oldValueOfIter =
            (exprs.empty())
                ? ViewRef<ViewType>(nullptr)
                : mpark::get<IterRef<ViewType>>(unrolledIterVals.back())
                      .getIterator()
                      .getValue();
        unrolledIterVals.emplace_back(iterRef);

        exprs.emplace_back(deepCopyForUnroll(exprToCopy, iterRef));
        iterRef.getIterator().changeValue(triggering() && !evaluateExpr,
                                          oldValueOfIter, newView, [&]() {
                                              if (evaluateExpr) {
                                                  evaluate(exprs.back());
                                              }
                                              if (triggering()) {
                                                  startTriggering(exprs.back());
                                              }
                                          });
        auto& triggerMember = exprs.back();
        visitTriggers([&](auto& t) { t->exprUnrolled(triggerMember); },
                      triggers);
    }

    inline void roll(u_int64_t index) {
        debug_log("Rolling  index " << index << " with value "
                                    << unrolledIterVals[index]);
        ExprRef<ExprType> removedExpr = std::move(exprs[index]);
        exprs[index] = std::move(exprs.back());
        unrolledIterVals[index] = std::move(unrolledIterVals.back());
        exprs.pop_back();
        unrolledIterVals.pop_back();
        visitTriggers([&](auto& t) { t->exprRolled(index, removedExpr); },
                      triggers);
    }

    inline std::shared_ptr<QuantifierView<ExprType>>
    deepCopyQuantifierForUnroll(const AnyIterRef& iterator) final {
        auto newQuantifier =
            std::make_shared<Quantifier<ContainerType, ExprType>>(
                deepCopyForUnroll(container, iterator), quantId);
        newQuantifier->expr = deepCopyForUnroll(expr, iterator);
        for (size_t i = 0; i < exprs.size(); ++i) {
            auto& expr = exprs[i];
            auto& iterVal = unrolledIterVals[i];
            newQuantifier->exprs.emplace_back(
                deepCopyForUnroll(expr, iterator));
            newQuantifier->unrolledIterVals.emplace_back(iterVal);
        }
        return newQuantifier;
    }

    inline void startTriggeringOnContainer() final {
        if (triggering()) {
            return;
        }
        containerTrigger =
            std::make_shared<ContainerTrigger<ContainerType, ExprType>>(this);
        addTrigger(container, containerTrigger);
        container->startTriggering();
    }
    inline void stopTriggeringOnContainer() final {
        if (containerTrigger) {
            deleteTrigger(containerTrigger);
            containerTrigger = nullptr;
            container->stopTriggering();
        }
    }
    inline void initialUnroll() final {
        unrolledIterVals.clear();
        exprs.clear();
        container->evaluate();
        InitialUnroller<ContainerType>::initialUnroll(*this);
    }
};

template <typename ReturnExprType>
struct ContainerTrigger<SetView, ReturnExprType> : public SetTrigger,
                                                   public DelayedTrigger {
    Quantifier<SetView, ReturnExprType>* op;
    AnyExprVec valuesToUnroll;

    ContainerTrigger(Quantifier<SetView, ReturnExprType>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                valuesToUnroll.emplace<BaseType<decltype(members)>>();
            },
            op->container.members);
    }
    inline void valueRemoved(u_int64_t indexOfRemovedValue, u_int64_t) final {
        op->roll(indexOfRemovedValue);
    }
    inline void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<exprType(vToUnroll)>>(member));
                if (vToUnroll.size() == 1) {
                    addDelayedTrigger(op->containerTrigger);
                }
            },
            valuesToUnroll);
    }

    inline void possibleMemberValueChange(u_int64_t, const AnyExprRef&) final {}
    inline void memberValueChanged(u_int64_t, const AnyExprRef&) final{};

    inline void setValueChanged(const SetView& newValue) {
        while (!op->exprs.empty()) {
            this->valueRemoved(op->exprs.size() - 1, 0);
        }
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    this->valueAdded(member);
                }
            },
            newValue.members);
    }

    inline void iterHasNewValue(const SetView&,
                                const ExprRef<SetView>& newValue) final {
        this->setValueChanged(*newValue);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                for (auto& value : vToUnroll) {
                    op->unroll(value);
                }
                vToUnroll.clear();
            },
            valuesToUnroll);
    }
};

template <>
struct InitialUnroller<SetView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier) {
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    quantifier.unroll(member);
                }
            },
            quantifier.container->members);
    }
};

template <typename ReturnExprType>
struct ContainerTrigger<MSetView, ReturnExprType> : public MSetTrigger,
                                                    public DelayedTrigger {
    Quantifier<MSetView, ReturnExprType>* op;
    AnyExprVec valuesToUnroll;

    ContainerTrigger(Quantifier<MSetView, ReturnExprType>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                valuesToUnroll.emplace<ExprRefVec<exprType(members)>>();
            },
            op->container.members);
    }
    inline void valueRemoved(u_int64_t indexOfRemovedValue, u_int64_t) final {
        op->roll(indexOfRemovedValue);
    }
    inline void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<exprType(vToUnroll)>>(member));
                if (vToUnroll.size() == 1) {
                    addDelayedTrigger(op->containerTrigger);
                }
            },
            valuesToUnroll);
    }

    inline void possibleMemberValueChange(u_int64_t, const AnyExprRef&) final {}
    inline void memberValueChanged(u_int64_t, const AnyExprRef&) final{};

    inline void mSetValueChanged(const MSetView& newValue) {
        while (!op->exprs.empty()) {
            this->valueRemoved(op->exprs.size() - 1, 0);
        }
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    this->valueAdded(member);
                }
            },
            newValue.members);
    }

    inline void iterHasNewValue(const MSetView&,
                                const ExprRef<MSetView>& newValue) final {
        this->mSetValueChanged(*newValue);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                for (auto& value : vToUnroll) {
                    op->unroll(value);
                }
                vToUnroll.clear();
            },
            valuesToUnroll);
    }
};

template <>
struct InitialUnroller<MSetView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier) {
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    quantifier.unroll(member);
                }
            },
            quantifier.container->members);
    }
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
