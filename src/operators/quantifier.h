#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
#include "operators/operatorBase.h"
#include "operators/quantifierView.h"
#include "types/typeOperations.h"

template <typename ContainerType, typename ExprType>
struct Quantifier : public QuantifierView<ExprType> {
    typedef typename AssociatedValueType<ExprType>::type ExprValueType;
    typedef
        typename AssociatedValueType<ContainerType>::type ContainerValueType;

    const int quantId;
    ContainerType container;
    ExprType expr = ValRef<ExprValueType>(nullptr);
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType>> containerTrigger;
    inline static u_int64_t nextQuantId() {
        static u_int64_t quantId = 0;
        return quantId++;
    }

    Quantifier(ContainerType container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}
    ~Quantifier() { stopTriggeringOnContainer(); }
    Quantifier(const Quantifier<ContainerType, ExprType>&& other)
        : QuantifierView(std::move(other)),
          quantId(other.quantId),
          container(std::move(other.container)),
          expr(std::move(other.expr)),
          unrolledIterVals(std::move(other.unrolledIterVals)),
          triggering()(other.triggering()),
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

                unrolledExprs.emplace_back(
                    deepCopyForUnroll(exprToCopy, iterRef));
                iterRef.getIterator().valueChange(
                    triggering(), oldValueOfIter, newValImpl, [&]() {
                        if (evaluateExpr) {
                            evaluate(unrolledExprs.back().first);
                        }
                        if (triggering()) {
                            startTriggering(unrolledExprs.back().first);
                        }
                    });
                visitTriggers([&](auto& t) { t->exprUnrolled(exprs.back()); },
                              triggers);
            },
            newValue);
    }

    inline void roll(u_int64_t index) {
        debug_log("Rolling " << unrolledIterVals[index]);
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
            std::make_shared<Quantifier<ContainerType, Exprtype>>(
                deepCopyForUnroll(container, iterator), quantId);
        newQuantifier->expr = deepCopyForUnroll(expr, iterator);
        // this next if statement temperarily disabled.
        // we want to identify if the unrolling value is our
        // container. i.e. this is the inner quantifier and the container we are
        // pointing to is going to be swapped out for a new one.
        // If so, we don't bother repopulating the unrolledExprs as they will be
        // repopulated when the new value of the container is chosen and a
        // trigger activated.
        const IterRef<ContainerValueType>* containerPtr =
            mpark::get_if<IterRef<ContainerValueType>>(&container);
        const IterRef<ContainerValueType>* iteratorPtr =
            mpark::get_if<IterRef<ContainerValueType>>(&iterator);
        if (false && containerPtr != NULL && iteratorPtr != NULL &&
            containerPtr->getIterator().id == iteratorPtr->getIterator().id) {
            return newQuantifier;
        }
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
        containerTrigger =
            std::make_shared<ContainerTrigger<ContainerType>>(this);
        addTrigger(*this, containerTrigger);
        startTriggering(container);
        triggering() = true;
    }
    inline void stopTriggeringOnContainer() final {
        if (containerTrigger) {
            deleteTrigger(containerTrigger);
            stopTriggering(container);
        }
        triggering() = false;
    }
    inline void initialUnroll() final {
        evaluate(container);
        InitialUnroller<ContainerType>::initialUnroll(*this);
    }

    template <typename Container>
    struct ContainerTrigger;
    template <typename T = int>  // template is there just to prevent compiler
                                 // from instantiating
    struct ContainerTrigger<SetReturning> : public SetTrigger, DelayedTrigger {
        Quantifier<ContainerType, ExprType>* op;
        std::vector<AnyValRef> valuesToUnroll;

        Quantifier(ContainerTrigger<containerType, ExprType>* op) op(op) {}
        inline void valueRemoved(u_int64_t indexOfRemovedValue) final {
            op->roll(indexOfRemovedValue);
        }
        inline void valueAdded(const AnyValRef& member) final {
            valuesToUnroll.emplace_back(move(val));
            if (op->valuesToUnroll.size() == 1) {
                addDelayedTrigger(op->containerTrigger);
            }
        }
        inline void possibleMemberValueChange(u_int64_t,
                                              const AnyValRef&) final {}
        inline void memberValueChanged(u_int64_t, const AnyValRef&) final{};

        inline void setValueChanged(const SetView& newValue) {
            while (!op->exprs.empty()) {
                this->valueremoved(op->exprs.size() - 1, 0);
            }
            mpark::visit(
                [&](auto& membersImpl) {
                    for (auto& member : membersImpl) {
                        this->valueAdded(member);
                    }
                },
                newValue.members);
        }

        inline void iterHasNewValue(const Setvalue&,
                                    const ValRef<SetValue>& newValue) final {
            this->setValueChanged(newValue);
        }
        void trigger() final {
            while (!valuesToUnroll.empty()) {
                op->unroll(valuesToUnroll.back());
                valuesToUnroll.pop_back();
            }
        }
    };

    template <typename Container>
    struct InitialUnroller;

    template <typename T = int>
    struct InitialUnrolleder<SetReturning> {
        template <typename Quant>
        void initialUnroll(Quant& quantifier) {
            mpark::visit(
                [&](auto& membersImpl) {
                    for (auto& member : membersImpl) {
                        quantifier.unrolled(member);
                    }
                },
                getView<SetView>(quant->container).members);
        }
    }
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
