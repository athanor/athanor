#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
#include "operators/operatorBase.h"
#include "operators/quantifierView.h"
#include "types/allTypes.h"
#include "base/typeOperations.h"
template <typename Container>
struct InitialUnroller;

template <typename Container, typename ExprType>
struct ContainerTrigger;

inline static u_int64_t nextQuantId() {
    static u_int64_t quantId = 0;
    return quantId++;
}

template <typename ContainerType, typename ExprType>
struct Quantifier : public QuantifierView<ExprType> {
    using QuantifierView<ExprType>::exprs;
    using QuantifierView<ExprType>::triggers;
    typedef typename AssociatedValueType<ExprType>::type ExprValueType;
    typedef
        typename AssociatedValueType<ContainerType>::type ContainerValueType;

    const int quantId;
    ContainerType container;
    ExprType expr = ValRef<ExprValueType>(nullptr);
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType, ExprType>> containerTrigger;

    Quantifier(ContainerType container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}
    ~Quantifier() { stopTriggeringOnContainer(); }
    Quantifier(const Quantifier<ContainerType, ExprType>&& other)
        : QuantifierView<ExprType>(std::move(other)),
          quantId(other.quantId),
          container(std::move(other.container)),
          expr(std::move(other.expr)),
          unrolledIterVals(std::move(other.unrolledIterVals)),
          containerTrigger(std::move(other.containerTrigger)) {
        setTriggerParent(this, containerTrigger);
    }

    inline void setExpression(ExprType exprIn) { expr = std::move(exprIn); }

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    inline bool triggering() { return static_cast<bool>(containerTrigger); }

    inline void unroll(const AnyValRef& newValue) {
        debug_log("unrolling " << newValue);
        mpark::visit(
            [&](auto& newValImpl) {
                typedef typename BaseType<decltype(newValImpl)>::element_type
                    IterValueType;
                auto iterRef = this->newIterRef<IterValueType>();
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
                    (exprs.empty()) ? ValRef<IterValueType>(nullptr)
                                    : mpark::get<IterRef<IterValueType>>(
                                          unrolledIterVals.back())
                                          .getIterator()
                                          .getValue();
                unrolledIterVals.emplace_back(iterRef);

                exprs.emplace_back(deepCopySelfForUnroll(exprToCopy, iterRef));
                iterRef.getIterator().changeValue(
                    triggering() && !evaluateExpr, oldValueOfIter, newValImpl,
                    [&]() {
                        if (evaluateExpr) {
                            evaluate(exprs.back());
                        }
                        if (triggering()) {
                            startTriggering(exprs.back());
                        }
                    });
                auto &triggerMember = exprs.back();
                visitTriggers([&](auto& t) { t->exprUnrolled(triggerMember); },
                              triggers);
            },
            newValue);
    }

    inline void roll(u_int64_t index) {
        debug_log("Rolling  index " << index << " with value "
                                    << unrolledIterVals[index]);
        ExprType removedExpr = std::move(exprs[index]);
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
                deepCopySelfForUnroll(container, iterator), quantId);
        newQuantifier->expr = deepCopySelfForUnroll(expr, iterator);
        for (size_t i = 0; i < exprs.size(); ++i) {
            auto& expr = exprs[i];
            auto& iterVal = unrolledIterVals[i];
            newQuantifier->exprs.emplace_back(
                deepCopySelfForUnroll(expr, iterator));
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

template <typename ExprType>
struct ContainerTrigger<ExprRef<SetView>, ExprType> : public SetTrigger,
                                                  public DelayedTrigger {
    Quantifier<ExprRef<SetView>, ExprType>* op;
    std::vector<AnyValRef> valuesToUnroll;

    ContainerTrigger(Quantifier<ExprRef<SetView>, ExprType>* op) : op(op) {}
    inline void valueRemoved(u_int64_t indexOfRemovedValue, u_int64_t) final {
        op->roll(indexOfRemovedValue);
    }
    inline void valueAdded(const AnyValRef& member) final {
        valuesToUnroll.emplace_back(std::move(member));
        if (valuesToUnroll.size() == 1) {
            addDelayedTrigger(op->containerTrigger);
        }
    }
    inline void possibleMemberValueChange(u_int64_t, const AnyValRef&) final {}
    inline void memberValueChanged(u_int64_t, const AnyValRef&) final{};

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

    inline void iterHasNewValue(const SetValue&,
                                const ValRef<SetValue>& newValue) final {
        this->setValueChanged(*newValue);
    }
    void trigger() final {
        for (auto& value : valuesToUnroll) {
            op->unroll(value);
        }
        valuesToUnroll.clear();
    }
};

template <>
struct InitialUnroller<ExprRef<SetView>> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier) {
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    quantifier.unroll(member);
                }
            },
            getView(quantifier.container).members);
    }
};

template <typename ExprType>
struct ContainerTrigger<MExprRef<SetView>, ExprType> : public MSetTrigger,
                                                   public DelayedTrigger {
    Quantifier<MExprRef<SetView>, ExprType>* op;
    std::vector<AnyValRef> valuesToUnroll;

    ContainerTrigger(Quantifier<MExprRef<SetView>, ExprType>* op) : op(op) {}
    inline void valueRemoved(u_int64_t indexOfRemovedValue, u_int64_t) final {
        op->roll(indexOfRemovedValue);
    }
    inline void valueAdded(const AnyValRef& member) final {
        valuesToUnroll.emplace_back(std::move(member));
        if (valuesToUnroll.size() == 1) {
            addDelayedTrigger(op->containerTrigger);
        }
    }
    inline void possibleMemberValueChange(u_int64_t, const AnyValRef&) final {}
    inline void memberValueChanged(u_int64_t, const AnyValRef&) final{};

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

    inline void iterHasNewValue(const MSetValue&,
                                const ValRef<MSetValue>& newValue) final {
        this->mSetValueChanged(*newValue);
    }
    void trigger() final {
        for (auto& value : valuesToUnroll) {
            op->unroll(value);
        }
        valuesToUnroll.clear();
    }
};

template <>
struct InitialUnroller<MExprRef<SetView>> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier) {
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    quantifier.unroll(member);
                }
            },
            getView(quantifier.container).members);
    }
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
