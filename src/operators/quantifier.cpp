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
Quantifier<ContainerType>::Quantifier(Quantifier<ContainerType>&& other)
    : SequenceView(move(other)),
      quantId(other.quantId),
      container(move(other.container)),
      expr(move(other.expr)),
      unrolledIterVals(move(other.unrolledIterVals)),
      containerTrigger(move(other.containerTrigger)),
      exprTriggers(move(other.exprTriggers)) {
    setTriggerParent(this, containerTrigger, exprTriggers);
}

template <typename ContainerType>
bool Quantifier<ContainerType>::triggering() {
    return static_cast<bool>(containerTrigger);
}

template <typename ContainerType>
template <typename ViewType>
void Quantifier<ContainerType>::unroll(UInt index,
                                       const ExprRef<ViewType>& newView) {
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
    ExprRef<ViewType> newMember =
        deepCopyForUnroll<viewType(members)>(exprToCopy, iterRef);
    iterRef.getIterator().changeValue(
        triggering() && !evaluateExpr, oldValueOfIter, newView, [&]() {
            if (evaluateExpr) {
                newMember->evaluate();
            }
            if (triggering()) {
                newMember->startTriggering();
                startTriggeringOnExpr(index, newMember);
            }
        });
    unrolledIterVals.insert(unrolledIterVals.begin() + index, iterRef);
    addMemberAndNotify(index, newMember);
}

template <typename ContainerType>
void Quantifier<ContainerType>::swap(UInt index1, UInt index2) {
    mpark::visit(
        [&](auto& members) {
            notifyBeginSwaps();
            swapAndNotify<viewType(members)>(index1, index2);
            notifyEndSwaps();
        },
        members);
    std::swap(exprTriggers[index1], exprTriggers[index2]);
    exprTriggers[index1]->index = index1;
    exprTriggers[index2]->index = index2;
    std::swap(unrolledIterVals[index1], unrolledIterVals[index2]);
}

template <typename ContainerType>
void Quantifier<ContainerType>::roll(UInt index) {
    debug_log("Rolling  index " << index << " with value "
                                << unrolledIterVals[index]);
    mpark::visit(
        [&](auto& members) {
            removeMemberAndNotify<viewType(members)>(index);
            if (triggering()) {
                stopTriggeringOnExpr<viewType(members)>(index);
            }
        },
        members);
    unrolledIterVals.erase(unrolledIterVals.begin() + index);
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
    void adapterPossibleValueChange() {
        mpark::visit(
            [&](auto& members) {
                this->op->template notifyPossibleSubsequenceChange<viewType(
                    members)>(this->index, this->index + 1);
            },
            this->op->members);
    }
    void adapterValueChanged() {
        mpark::visit(
            [&](auto& members) {
                this->op
                    ->template changeSubsequenceAndNotify<viewType(members)>(
                        this->index, this->index + 1);
            },
            this->op->members);
    }
    ExprChangeTrigger() {}
};

template <typename ContainerType>
template <typename ViewType>
void Quantifier<ContainerType>::startTriggeringOnExpr(UInt index,
                                                      ExprRef<ViewType>& expr) {
    auto trigger =
        make_shared<ExprChangeTrigger<ContainerType, viewType(expr)>>(this,
                                                                      index);
    exprTriggers.insert(exprTriggers.begin() + index, trigger);
    for (size_t i = index + 1; i < exprTriggers.size(); i++) {
        exprTriggers[i]->index = i;
    }
    addTrigger(expr, trigger);
}

template <typename ContainerType>
template <typename ViewType>
void Quantifier<ContainerType>::stopTriggeringOnExpr(UInt oldIndex) {
    deleteTrigger(
        static_pointer_cast<ExprChangeTrigger<ContainerType, ViewType>>(
            exprTriggers[oldIndex]));
    exprTriggers.erase(exprTriggers.begin() + oldIndex);
    for (size_t i = oldIndex; i < exprTriggers.size(); i++) {
        exprTriggers[i]->index = i;
    }
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
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); i++) {
                startTriggeringOnExpr<viewType(members)>(i, members[i]);
                members[i]->startTriggering();
            }
        },
        members);
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggering() {
    if (containerTrigger) {
        deleteTrigger(containerTrigger);
        containerTrigger = nullptr;
        container->stopTriggering();
        mpark::visit(
            [&](auto& expr) {
                while (!exprTriggers.empty()) {
                    stopTriggeringOnExpr<viewType(expr)>(exprTriggers.size() -
                                                         1);
                }
            },
            expr);
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
    void possibleSetValueChange() final {}
    void valueRemoved(UInt indexOfRemovedValue, HashType) final {
        if (indexOfRemovedValue < op->numberElements() - 1) {
            op->swap(indexOfRemovedValue, op->numberElements() - 1);
        }
        op->roll(op->numberElements() - 1);
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
    void preIterValueChange(const ExprRef<SetView>&) final {
        this->possibleSetValueChange();
    }
    void postIterValueChange(const ExprRef<SetView>& newValue) final {
        this->setValueChanged(*newValue);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                for (auto& value : vToUnroll) {
                    op->unroll(op->numberElements(), value);
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
                    quantifier.unroll(quantifier.numberElements(), member);
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
        if (indexOfRemovedValue < op->numberElements() - 1) {
            op->swap(indexOfRemovedValue, op->numberElements() - 1);
        }
        op->roll(op->numberElements() - 1);
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
    void possibleMSetValueChange() final {}
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

    void preIterValueChange(const ExprRef<MSetView>&) final {
        this->possibleMSetValueChange();
    }
    void postIterValueChange(const ExprRef<MSetView>& newValue) final {
        this->mSetValueChanged(*newValue);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                for (auto& value : vToUnroll) {
                    op->unroll(op->numberElements(), value);
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
                    quantifier.unroll(quantifier.numberElements(), member);
                }
            },
            quantifier.container->members);
    }
};

vector<ExprRef<SequenceView>> instantiateQuantifierTypes() {
    vector<ExprRef<SequenceView>> quants;
    auto q1 = make_shared<Quantifier<SetView>>(ViewRef<SetView>(nullptr));
    q1->setExpression(ViewRef<SetView>(nullptr));
    quants.emplace_back(ViewRef<SequenceView>(q1));
    auto q2 = make_shared<Quantifier<MSetView>>(ViewRef<MSetView>(nullptr));
    q2->setExpression(ViewRef<MSetView>(nullptr));
    quants.emplace_back(ViewRef<SequenceView>(q2));
    return quants;
}
