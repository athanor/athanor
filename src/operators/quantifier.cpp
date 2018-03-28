#ifndef SRC_OPERATORS_QUANTIFIER_H_
#include "operators/quantifierView.h"
#define SRC_OPERATORS_QUANTIFIER_H_
#include "base/base.h"
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

template <typename ContainerType>
struct Quantifier : public SequenceView {
    const int quantId;
    ExprRef<ContainerType> container;
    AnyExprRef expr;
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType, ExprType>> containerTrigger;

    Quantifier(ExprRef<ContainerType> container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}
    ~Quantifier() { stopTriggeringOnContainer(); }
    Quantifier(const Quantifier<ContainerType, ExprRef<ExprType>>&& other)
        : SequenceView(std::move(other)),
          quantId(other.quantId),
          container(std::move(other.container)),
          expr(std::move(other.expr)),
          unrolledIterVals(std::move(other.unrolledIterVals)),
          containerTrigger(std::move(other.containerTrigger)) {
        setTriggerParent(this, containerTrigger);
    }

    inline void setExpression(AnyExprRef exprIn) {
        expr = std::move(exprIn);
        mpark::visit(
            [&](auto& expr) { members.emplace<ExprRefVec<viewType(expr)>>(); },
            expr);
    }

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    inline bool triggering() { return static_cast<bool>(containerTrigger); }
    template <typename ViewType>
    inline void unroll(const ExprRef<ViewType>& newView) {
        auto& members = getMembers<ViewType>();
        debug_log("unrolling " << newView);
        auto iterRef = this->newIterRef<ViewType>();
        // choose which expr to copy from
        // use previously unrolled expr if available as  can be most
        // efficiently updated to new unrolled value  otherwise use expr
        // template
        auto& exprToCopy = (members.empty()) ? expr : members.back();
        // decide if expr that is going to be unrolled needs evaluating.
        // It needs evaluating if we are in triggering() mode and if the
        // expr being unrolled is taken from the expr template, because the
        // expr will not have been evaluated before.  If we are instead
        // copying from an already unrolled expr, it  need not be
        // evaluated, simply copy it and trigger the change in value.
        bool evaluateExpr = members.empty() && triggering();
        const ExprRef<ViewType>& oldValueOfIter =
            (members.empty())
                ? ViewRef<ViewType>(nullptr)
                : mpark::get<IterRef<ViewType>>(unrolledIterVals.back())
                      .getIterator()
                      .getValue();
        unrolledIterVals.emplace_back(iterRef);
        ExprRef<ViewType> newMember = deepCopyForUnroll(exprToCopy, iterRef);
        iterRef.getIterator().changeValue(
            triggering() && !evaluateExpr, oldValueOfIter, newView, [&]() {
                if (evaluateExpr) {
                    members.back()->evaluate();
                }
                if (triggering()) {
                    members.back()->startTriggering();
                }
            });
        addMemberAndNotify(members.size(), newMember);
    }

    inline void roll(UInt index) {
        debug_log("Rolling  index " << index << " with value "
                                    << unrolledIterVals[index]);
        notifyBeginSwaps();
        mpark::visit(
            [&](auto& members) {
                swapAndNotify<viewType(members)>(index, members.size() - 1);
                notifyEndSwaps();
                removedMemberAndNotify(members.size() - 1);
            },
            members);
        unrolledIterVals[index] = std::move(unrolledIterVals.back());
        unrolledIterVals.pop_back();
    }

    inline ExprRef<SequenceView> deepCopyForUnroll(
        const AnyIterRef& iterator) final final {
        auto newQuantifier =
            std::make_shared<Quantifier<ContainerType, ExprType>>(
                deepCopyForUnroll(container, iterator), quantId);
        newQuantifier->expr = deepCopyForUnroll(expr, iterator);

        mpark::visit(
            [&](auto& members) {
                newQuantifier->members.emplace<viewType(members)>();
                for (size_t i = 0; i < members.size(); ++i) {
                    auto& expr = members[i];
                    auto& iterVal = unrolledIterVals[i];
                    newQuantifier.addMember<viewType(members)>(
                        deepCopyForUnroll(expr, iterator));
                    newQuantifier->unrolledIterVals.emplace_back(iterVal);
                }
            },
            members);
        return newQuantifier;
    }

    inline void startTriggering() final {
        if (triggering()) {
            return;
        }
        containerTrigger =
            std::make_shared<ContainerTrigger<ContainerType, ExprType>>(this);
        addTrigger(container, containerTrigger);
        container->startTriggering();
    }
    inline void stopTriggering() final {
        if (containerTrigger) {
            deleteTrigger(containerTrigger);
            containerTrigger = nullptr;
            container->stopTriggering();
        }
    }
    inline void evaluate() final {
        unrolledIterVals.clear();
        silentClear();
        container->evaluate();
        InitialUnroller<ContainerType>::initialUnroll(*this);
    }
    std::ostream& dumpState(std::ostream& os) const final {
        mpark::visit([&] (auto& members) {

        })
    }
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription& vioDesc) final {
        container->updateViolationDescription(parentViolation, vioDesc);
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
            op->container->members);
    }
    inline void valueRemoved(UInt indexOfRemovedValue, HashType) final {
        op->roll(indexOfRemovedValue);
    }
    inline void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<viewType(vToUnroll)>>(member));
                if (vToUnroll.size() == 1) {
                    addDelayedTrigger(op->containerTrigger);
                }
            },
            valuesToUnroll);
    }

    inline void possibleMemberValueChange(UInt, const AnyExprRef&) final {}
    inline void memberValueChanged(UInt, const AnyExprRef&) final{};

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
                valuesToUnroll.emplace<ExprRefVec<viewType(members)>>();
            },
            op->container->members);
    }
    inline void valueRemoved(UInt indexOfRemovedValue, HashType) final {
        op->roll(indexOfRemovedValue);
    }
    inline void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<viewType(vToUnroll)>>(member));
                if (vToUnroll.size() == 1) {
                    addDelayedTrigger(op->containerTrigger);
                }
            },
            valuesToUnroll);
    }

    inline void possibleMemberValueChange(UInt, const AnyExprRef&) final {}
    inline void memberValueChanged(UInt, const AnyExprRef&) final{};

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
