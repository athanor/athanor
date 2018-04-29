#include "operators/quantifier.h"

#include <vector>
#include "operators/opTupleLit.h"
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
template <>
struct InitialUnroller<SequenceView>;

template <typename Container>
struct ContainerTrigger;
template <>
struct ContainerTrigger<SetView>;
template <>
struct ContainerTrigger<MSetView>;
template <>
struct ContainerTrigger<SequenceView>;

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
    mpark::visit(
        [&](auto& members) {
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
            // It needs evaluating if we are not in triggering  mode or if
            // the expr being unrolled is taken from the expr template, because
            // the expr template will not have been evaluated before.  If we are
            // instead copying from an already unrolled expr, it  need not be
            // evaluated, simply copy it and trigger the change in value.
            bool evaluateExpr = members.empty() || !this->triggering();
            const ExprRef<ViewType>& oldValueOfIter =
                (members.empty())
                    ? IterRef<ViewType>(nullptr)
                    : mpark::get<IterRef<ViewType>>(unrolledIterVals.back())
                          ->getValue();
            ExprRef<viewType(members)> newMember =
                exprToCopy->deepCopySelfForUnroll(exprToCopy, iterRef);
            iterRef->changeValue(this->triggering() && !evaluateExpr,
                                 oldValueOfIter, newView, [&]() {
                                     if (evaluateExpr) {
                                         newMember->evaluate();
                                     }
                                     if (this->triggering()) {
                                         newMember->startTriggering();
                                     }
                                 });
            unrolledIterVals.insert(unrolledIterVals.begin() + index, iterRef);
            this->addMemberAndNotify(index, newMember);
            if (this->triggering()) {
                this->startTriggeringOnExpr(index, newMember);
            }

        },
        members);
}

template <typename ContainerType>
void Quantifier<ContainerType>::swap(UInt index1, UInt index2,
                                     bool notifyBeginEnd) {
    mpark::visit(
        [&](auto& members) {
            if (notifyBeginEnd) {
                this->notifyBeginSwaps();
            }
            this->swapAndNotify<viewType(members)>(index1, index2);
            if (notifyBeginEnd) {
                this->notifyEndSwaps();
            }
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
            this->removeMemberAndNotify<viewType(members)>(index);
            if (this->triggering()) {
                this->stopTriggeringOnExpr<viewType(members)>(index);
            }
        },
        members);
    unrolledIterVals.erase(unrolledIterVals.begin() + index);
}

template <typename ContainerType>
ExprRef<SequenceView> Quantifier<ContainerType>::deepCopySelfForUnroll(
    const ExprRef<SequenceView>&, const AnyIterRef& iterator) const {
    auto newQuantifier = make_shared<Quantifier<ContainerType>>(
        container->deepCopySelfForUnroll(container, iterator), quantId);

    mpark::visit(
        [&](const auto& expr) {
            newQuantifier->setExpression(
                expr->deepCopySelfForUnroll(expr, iterator));
            auto& members = this->getMembers<viewType(expr)>();
            for (size_t i = 0; i < members.size(); ++i) {
                auto& member = members[i];
                auto& iterVal = unrolledIterVals[i];
                newQuantifier->template addMember<viewType(members)>(
                    newQuantifier->numberElements(),
                    member->deepCopySelfForUnroll(member, iterator));
                newQuantifier->unrolledIterVals.emplace_back(iterVal);
            }
        },
        this->expr);
    return newQuantifier;
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
    void reattachTrigger() final {
        auto& triggerToChange = this->op->exprTriggers.at(this->index);
        deleteTrigger(
            static_pointer_cast<ExprChangeTrigger<ContainerType, ViewType>>(
                triggerToChange));
        auto trigger = make_shared<ExprChangeTrigger<ContainerType, ViewType>>(
            this->op, this->index);
        this->op->template getMembers<ViewType>()[this->index]->addTrigger(
            trigger);
        triggerToChange = trigger;
    }
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
    expr->addTrigger(trigger);
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
    if (this->triggering()) {
        return;
    }
    containerTrigger = make_shared<ContainerTrigger<ContainerType>>(this);
    container->addTrigger(containerTrigger);
    container->startTriggering();
    mpark::visit(
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); i++) {
                this->startTriggeringOnExpr<viewType(members)>(i, members[i]);
                members[i]->startTriggering();
            }
        },
        members);
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggeringOnChildren() {
    if (containerTrigger) {
        deleteTrigger(containerTrigger);
        containerTrigger = nullptr;
        mpark::visit(
            [&](auto& expr) {
                while (!exprTriggers.empty()) {
                    this->stopTriggeringOnExpr<viewType(expr)>(
                        exprTriggers.size() - 1);
                }
            },
            expr);
    }
}

template <typename ContainerType>
void Quantifier<ContainerType>::stopTriggering() {
    if (containerTrigger) {
        stopTriggeringOnChildren();
        mpark::visit(
            [&](auto& expr) {
                for (auto& member : this->getMembers<viewType(expr)>()) {
                    member->stopTriggering();
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

template <typename ContainerType>
void Quantifier<ContainerType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    mpark::visit([&](auto& expr) { expr = findAndReplace(expr, func); },
                 this->expr);
}

template <>
struct ContainerTrigger<SetView> : public SetTrigger, public DelayedTrigger {
    Quantifier<SetView>* op;
    AnyExprVec valuesToUnroll;

    ContainerTrigger(Quantifier<SetView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                valuesToUnroll.emplace<BaseType<decltype(members)>>();
            },
            op->container->view().members);
    }
    void possibleValueChange() final {}
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

    void valueChanged() {
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1, 0);
        }
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    this->valueAdded(member);
                }
            },
            op->container->view().members);
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
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<SetView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
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
            quantifier.container->view().members);
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
            op->container->view().members);
    }
    void valueRemoved(UInt indexOfRemovedValue, const AnyExprRef&) final {
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
    void possibleValueChange() final {}
    void valueChanged() {
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1,
                               ExprRef<BoolView>(nullptr));
        }
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    this->valueAdded(member);
                }
            },
            op->container->view().members);
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
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<MSetView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
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
            quantifier.container->view().members);
    }
};

template <typename Op>
struct OpMaker;
template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(std::vector<AnyExprRef> members);
};

template <>
struct ContainerTrigger<SequenceView> : public SequenceTrigger,
                                        public DelayedTrigger {
    Quantifier<SequenceView>* op;
    vector<UInt> indicesOfValuesToUnroll;
    AnyExprVec valuesToUnroll;

    ContainerTrigger(Quantifier<SequenceView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                valuesToUnroll.emplace<ExprRefVec<viewType(members)>>();
            },
            op->container->view().members);
    }
    void valueRemoved(UInt indexOfRemovedValue, const AnyExprRef&) final {
        op->roll(indexOfRemovedValue);
        correctUnrolledTupleIndices(indexOfRemovedValue);
    }
    void valueAdded(UInt index, const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<viewType(vToUnroll)>>(member));
                indicesOfValuesToUnroll.emplace_back(index);
                if (vToUnroll.size() == 1) {
                    addDelayedTrigger(op->containerTrigger);
                }
            },
            valuesToUnroll);
    }

    void possibleSubsequenceChange(UInt, UInt) final {}
    void subsequenceChanged(UInt, UInt) final{};
    void possibleValueChange() final {}
    void valueChanged() {
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1,
                               ExprRef<BoolView>(nullptr));
        }
        indicesOfValuesToUnroll.clear();
        mpark::visit(
            [&](auto& membersImpl) {
                auto& vToUnroll = mpark::get<ExprRefVec<viewType(membersImpl)>>(
                    valuesToUnroll);
                indicesOfValuesToUnroll.clear();
                vToUnroll.clear();
                for (auto& member : membersImpl) {
                    this->valueAdded(vToUnroll.size(), member);
                }
            },
            op->container->view().members);
    }

    void beginSwaps() final { op->notifyBeginSwaps(); }
    void endSwaps() final { op->notifyEndSwaps(); }
    void positionsSwapped(UInt index1, UInt index2) final {
        op->swap(index1, index2, false);
        correctUnrolledTupleIndex(index1);
        correctUnrolledTupleIndex(index2);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                debug_code(
                    assert(vToUnroll.size() == indicesOfValuesToUnroll.size()));
                const UInt MAX_UINT = ~((UInt)0);
                UInt minIndex = MAX_UINT;
                for (size_t i = 0; i < vToUnroll.size(); i++) {
                    auto tupleFirstMember = make<IntValue>();
                    tupleFirstMember->value = indicesOfValuesToUnroll[i] + 1;
                    auto unrolledExpr = OpMaker<OpTupleLit>::make(
                        {tupleFirstMember.asExpr(), vToUnroll[i]});
                    op->unroll(indicesOfValuesToUnroll[i], unrolledExpr);
                    if (indicesOfValuesToUnroll[i] + 1 <
                            op->unrolledIterVals.size() &&
                        indicesOfValuesToUnroll[i] + 1 < minIndex) {
                        minIndex = indicesOfValuesToUnroll[i] + 1;
                    }
                }
                vToUnroll.clear();
                indicesOfValuesToUnroll.clear();
                if (minIndex != MAX_UINT) {
                    correctUnrolledTupleIndices(minIndex);
                }
            },
            valuesToUnroll);
    }
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<SequenceView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }

    void correctUnrolledTupleIndices(size_t startIndex) {
        for (size_t i = startIndex; i < op->unrolledIterVals.size(); i++) {
            correctUnrolledTupleIndex(i);
        }
    }
    void correctUnrolledTupleIndex(size_t index) {
        auto& tuple =
            mpark::get<IterRef<TupleView>>(op->unrolledIterVals[index]);
        auto& intView = mpark::get<ExprRef<IntView>>(tuple->view().members[0]);
        auto& intVal = static_cast<IntValue&>(*intView);
        intVal.changeValue([&]() {
            intVal.value = index + 1;
            return true;
        });
    }
};

template <>
struct InitialUnroller<SequenceView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier) {
        mpark::visit(
            [&](auto& membersImpl) {
                for (size_t i = 0; i < membersImpl.size(); i++) {
                    auto tupleFirstMember = make<IntValue>();
                    tupleFirstMember->value = i + 1;
                    auto unrolledExpr = OpMaker<OpTupleLit>::make(
                        {tupleFirstMember.asExpr(), membersImpl[i]});
                    quantifier.unroll(i, unrolledExpr);
                }
            },
            quantifier.container->view().members);
    }
};

template struct Quantifier<SetView>;
template struct Quantifier<MSetView>;
template struct Quantifier<SequenceView>;
