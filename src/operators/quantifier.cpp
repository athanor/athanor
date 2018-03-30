#include "operators/quantifier.h"
#include "types/allTypes.h"
using namespace std;
template <typename ContainerType>
struct InitialUnroller;

template <>
struct InitialUnroller<MSetView>;

template <>
struct InitialUnroller<SetView>;
template <>
struct InitialUnroller<MSetView>;
template <typename Container>
struct ContainerTrigger;
template <>
struct ContainerTrigger<SetView>;
template <>
struct ContainerTrigger<MSetView>;

template <typename ContainerType>
Quantifier<ContainerType>::Quantifier(ExprRef<ContainerType> container,
                                      const int id)
    : quantId(id), container(move(container)) {}

template <typename ContainerType>
Quantifier<ContainerType>::Quantifier(const Quantifier<ContainerType>&& other)
    : SequenceView(move(other)),
      quantId(other.quantId),
      container(move(other.container)),
      expr(move(other.expr)),
      unrolledIterVals(move(other.unrolledIterVals)),
      containerTrigger(move(other.containerTrigger)),
      exprTrigger(move(other.exprTrigger)) {
    setTriggerParent(this, containerTrigger, exprTrigger);
}

template <typename ContainerType>
void Quantifier<ContainerType>::setExpression(AnyExprRef exprIn) {
    expr = move(exprIn);
    mpark::visit(
        [&](auto& expr) { members.emplace<ExprRefVec<viewType(expr)>>(); },
        expr);
}

template <typename ContainerType>
bool Quantifier<ContainerType>::triggering() {
    return static_cast<bool>(containerTrigger);
}

template <typename ContainerType>
template <typename ViewType>
void Quantifier<ContainerType>::unroll(const ExprRef<ViewType>& newView) {
    auto& members = getMembers<ViewType>();
    debug_log("unrolling " << newView);
    auto iterRef = this->newIterRef<ViewType>();
    // choose which expr to copy from
    // use previously unrolled expr if available as  can be most
    // efficiently updated to new unrolled value  otherwise use expr
    // template
    auto exprToCopy = (members.empty())
                          ? mpark::get<ExprRef<viewType(members)>>(expr)
                          : members.back();
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
    ExprRef<ViewType> newMember =
        deepCopyForUnroll<viewType(members)>(exprToCopy, iterRef);
    iterRef.getIterator().changeValue(triggering() && !evaluateExpr,
                                      oldValueOfIter, newView, [&]() {
                                          if (evaluateExpr) {
                                              members.back()->evaluate();
                                          }
                                          if (triggering()) {
                                              members.back()->startTriggering();
                                          }
                                      });
    addMemberAndNotify(members.size(), newMember);
    if (triggering()) {
        startTriggeringOnExpr(newMember);
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::roll(UInt index) {
    debug_log("Rolling  index " << index << " with value "
                                << unrolledIterVals[index]);
    mpark::visit(
        [&](auto& members) {
            notifyBeginSwaps();
            swapAndNotify<viewType(members)>(index, members.size() - 1);
            notifyEndSwaps();
            removeMemberAndNotify<viewType(members)>(members.size() - 1);
        },
        members);
    unrolledIterVals[index] = move(unrolledIterVals.back());
    unrolledIterVals.pop_back();
}

template <typename ContainerType>
ExprRef<SequenceView> Quantifier<ContainerType>::deepCopySelfForUnroll(
    const AnyIterRef& iterator) const {
    auto newQuantifier = make_shared<Quantifier<ContainerType>>(
        deepCopyForUnroll(container, iterator), quantId);
    newQuantifier->setExpression(deepCopyForUnroll(expr, iterator));

    mpark::visit(
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); ++i) {
                auto& expr = members[i];
                auto& iterVal = unrolledIterVals[i];
                newQuantifier->template addMember<viewType(members)>(
                    newQuantifier->numberElements(),
                    deepCopyForUnroll(expr, iterator));
                newQuantifier->unrolledIterVals.emplace_back(iterVal);
            }
        },
        members);
    return ViewRef<SequenceView>(newQuantifier);
}

template <typename ContainerType, typename ViewType>
struct ExprChangeTrigger
    : Quantifier<ContainerType>::ExprTriggerBase,
      ChangeTriggerAdapter<typename AssociatedTriggerType<ViewType>::type,
                           ExprChangeTrigger<ContainerType, ViewType>> {
    using Quantifier<ContainerType>::ExprTriggerBase ::ExprTriggerBase;
    void adapterValueChanged() {
        this->op->notifyUnknownSubsequenceChange();
        ;
    }
    ExprChangeTrigger() {}
};

ExprChangeTrigger<BoolView, BoolView> b();
template <typename ContainerType>
template <typename ViewType>
void Quantifier<ContainerType>::startTriggeringOnExpr(ExprRef<ViewType>& expr) {
    addTrigger(expr,
               static_pointer_cast<ExprChangeTrigger<ContainerType, ViewType>>(
                   exprTrigger));
}

template <typename ContainerType>
void Quantifier<ContainerType>::startTriggering() {
    if (triggering()) {
        return;
    }
    containerTrigger = make_shared<ContainerTrigger<ContainerType>>(this);
    addTrigger(container, containerTrigger);
    container->startTriggering();
    mpark::visit(
        [&](auto& expr) {
            auto trigger =
                make_shared<ExprChangeTrigger<ContainerType, viewType(expr)>>(
                    this);
            exprTrigger = trigger;
            for (auto& unrolledExpr :
                 this->template getMembers<viewType(expr)>()) {
                addTrigger(unrolledExpr, trigger);
            }
        },
        expr);
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggering() {
    if (containerTrigger) {
        deleteTrigger(containerTrigger);
        containerTrigger = nullptr;
        container->stopTriggering();
        mpark::visit(
            [&](auto& expr) {
                deleteTrigger(static_pointer_cast<
                              ExprChangeTrigger<ContainerType, viewType(expr)>>(
                    exprTrigger));
            },
            expr);
        exprTrigger = nullptr;
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::evaluate() {
    unrolledIterVals.clear();
    silentClear();
    container->evaluate();
    InitialUnroller<ContainerType>::initialUnroll(*this);
}
template <typename ContainerType>
ostream& Quantifier<ContainerType>::dumpState(ostream& os) const {
    mpark::visit(
        [&](auto& members) {
            os << "quantifier(container=";
            container->dumpState(os) << ",\n";
            os << "unrolledExprs=[";
            bool first = true;
            for (auto& unrolledExpr : members) {
                if (first) {
                    first = false;
                } else {
                    os << ",\n";
                }
                unrolledExpr->dumpState(os);
            }
            os << "]";
        },
        this->members);
    return os;
}

template <typename ContainerType>
void Quantifier<ContainerType>::updateViolationDescription(
    UInt, ViolationDescription&) {}

template <>
struct ContainerTrigger<SetView> : public SetTrigger, public DelayedTrigger {
    Quantifier<SetView>* op;
    AnyExprVec valuesToUnroll;

    ContainerTrigger(Quantifier<SetView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                valuesToUnroll.emplace<BaseType<decltype(members)>>();
            },
            op->container->members);
    }
    void valueRemoved(UInt indexOfRemovedValue, HashType) final {
        op->roll(indexOfRemovedValue);
    }
    void valueAdded(const AnyExprRef& member) final {
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

    void possibleMemberValueChange(UInt, const AnyExprRef&) final {}
    void memberValueChanged(UInt, const AnyExprRef&) final{};

    void setValueChanged(const SetView& newValue) {
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1, 0);
        }
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    this->valueAdded(member);
                }
            },
            newValue.members);
    }

    void iterHasNewValue(const SetView&,
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

template <>
struct ContainerTrigger<MSetView> : public MSetTrigger, public DelayedTrigger {
    Quantifier<MSetView>* op;
    AnyExprVec valuesToUnroll;

    ContainerTrigger(Quantifier<MSetView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                valuesToUnroll.emplace<ExprRefVec<viewType(members)>>();
            },
            op->container->members);
    }
    void valueRemoved(UInt indexOfRemovedValue, HashType) final {
        op->roll(indexOfRemovedValue);
    }
    void valueAdded(const AnyExprRef& member) final {
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

    void possibleMemberValueChange(UInt, const AnyExprRef&) final {}
    void memberValueChanged(UInt, const AnyExprRef&) final{};

    void mSetValueChanged(const MSetView& newValue) {
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1, 0);
        }
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    this->valueAdded(member);
                }
            },
            newValue.members);
    }

    void iterHasNewValue(const MSetView&,
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

void instantiateQuantifierTypes() {
    Quantifier<SetView> q1(ViewRef<SetView>(nullptr));
    Quantifier<MSetView> q2(ViewRef<MSetView>(nullptr));
    ignoreUnused(q1, q2);
}
