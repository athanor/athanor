#include "operators/quantifier.h"

#include <vector>
#include "operators/intRange.h"
#include "operators/opAnd.h"
#include "operators/opIntEq.h"
#include "operators/opLess.h"
#include "operators/opLessEq.h"
#include "operators/opProd.h"
#include "operators/opSequenceLit.h"
#include "operators/opSum.h"
#include "operators/opToInt.h"
#include "operators/opTupleIndex.h"
#include "operators/opTupleLit.h"
#include "types/allTypes.h"

template <typename Op>
struct OpMaker;
template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(std::vector<AnyExprRef> members);
};
template <bool minMode>
struct OpMinMax;
typedef OpMinMax<true> OpMin;
typedef OpMinMax<false> OpMax;

template <bool minMode>
struct OpMaker<OpMinMax<minMode>> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};

struct OpSequenceLit;
template <>
struct OpMaker<OpSequenceLit> {
    static ExprRef<SequenceView> make(AnyExprVec members);
};
struct OpSum;
template <>
struct OpMaker<OpSum> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};
struct OpMinus;
template <>
struct OpMaker<OpMinus> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

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

template <>
struct InitialUnroller<FunctionView>;

template <typename Container>
struct ContainerTrigger;
template <>
struct ContainerTrigger<SetView>;
template <>
struct ContainerTrigger<MSetView>;
template <>
struct ContainerTrigger<SequenceView>;
template <>
struct ContainerTrigger<FunctionView>;

template <typename ContainerType>
Quantifier<ContainerType>::Quantifier(Quantifier<ContainerType>&& other)
    : SequenceView(move(other)),
      quantId(other.quantId),
      container(move(other.container)),
      expr(move(other.expr)),
      unrolledIterVals(move(other.unrolledIterVals)),
      containerTrigger(move(other.containerTrigger)),
      containerDelayedTrigger(move(other.containerDelayedTrigger)),
      exprTriggers(move(other.exprTriggers)) {
    setTriggerParent(this, containerTrigger, containerDelayedTrigger,
                     exprTriggers);
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
            debug_log("unrolling " << newView << " at index " << index);
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
            bool evaluateExpr = members.empty();
            const ExprRef<ViewType>& oldValueOfIter =
                (members.empty())
                    ? IterRef<ViewType>(nullptr)
                    : mpark::get<IterRef<ViewType>>(unrolledIterVals.back())
                          ->getValue();
            ExprRef<viewType(members)> newMember =
                exprToCopy->deepCopySelfForUnroll(exprToCopy, iterRef);
            iterRef->changeValue(!evaluateExpr, oldValueOfIter, newView, [&]() {
                if (evaluateExpr) {
                    newMember->evaluate();
                }
                newMember->startTriggering();
            });
            unrolledIterVals.insert(unrolledIterVals.begin() + index, iterRef);
            this->addMemberAndNotify(index, newMember);
            this->startTriggeringOnExpr(index, newMember);
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
ExprRef<SequenceView> Quantifier<ContainerType>::deepCopySelfForUnrollImpl(
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
    newQuantifier->containerDefined = containerDefined;
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

    void adapterHasBecomeDefined() {
        mpark::visit(
            [&](auto& members) {
                this->op->template notifyMemberDefined<viewType(members)>(
                    this->index);
            },
            this->op->members);
    }
    void adapterHasBecomeUndefined() {
        mpark::visit(
            [&](auto& members) {
                this->op->template notifyMemberUndefined<viewType(members)>(
                    this->index);
            },
            this->op->members);
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
void Quantifier<ContainerType>::startTriggeringImpl() {
    if (!containerTrigger) {
        containerTrigger = make_shared<ContainerTrigger<ContainerType>>(this);
        container->addTrigger(containerTrigger);
        container->startTriggering();
    }
    if (!exprTriggers.empty()) {
        return;
    }
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
    }
    if (containerDelayedTrigger) {
        deleteTrigger(containerDelayedTrigger);
        containerDelayedTrigger = nullptr;
    }
    if (!exprTriggers.empty()) {
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
void Quantifier<ContainerType>::evaluateImpl() {
    debug_code(assert(unrolledIterVals.empty()));
    debug_code(assert(this->numberElements() == 0));
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
void Quantifier<ContainerType>::updateVarViolations(const ViolationContext&,
                                                    ViolationContainer&) {}

template <typename ContainerType>
void Quantifier<ContainerType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    mpark::visit([&](auto& expr) { expr = findAndReplace(expr, func); },
                 this->expr);
}

template <typename ContainerType>
bool Quantifier<ContainerType>::isUndefined() {
    return this->numberUndefined > 0 || !containerDefined;
}

template <typename ContainerType>
pair<bool, ExprRef<SequenceView>> Quantifier<ContainerType>::optimise(
    PathExtension path) {
    bool changeMade = false;
    auto optResult = container->optimise(path.extend(container));
    changeMade |= optResult.first;
    container = optResult.second;
    if (condition) {
        auto optResult = condition->optimise(path.extend(condition));
        changeMade |= optResult.first;
        condition = optResult.second;
    }
    mpark::visit(overloaded(
                     [&](ExprRef<IntView>& expr) {
                         auto optResult = expr->optimise(path.extend(expr));
                         changeMade |= optResult.first;
                         expr = optResult.second;
                         bool optimised =
                             optimiseIfOpSumParentWithZeroingCondition(
                                 *this, expr, path);
                         changeMade |= optimised;
                         optimised = optimiseIfIntRangeWithConditions(*this);
                         changeMade |= optimised;
                         if (optimised) {
                             container->optimise(path.extend(container));
                             expr->optimise(path.extend(expr));
                         }
                     },
                     [&](auto& expr) {
                         auto optResult = expr->optimise(path.extend(expr));
                         changeMade |= optResult.first;
                         expr = optResult.second;
                     }),
                 expr);
    if (condition) {
        cerr << "Error, for the moment, not supporting conditions on "
                "quantifiers unless they can be optimised into the an integer "
                "range.\n";
        abort();
    }
    return make_pair(changeMade, mpark::get<ExprRef<SequenceView>>(path.expr));
}

bool isOpSum(const AnyExprRef& expr) {
    auto intExprTest = mpark::get_if<ExprRef<IntView>>(&expr);
    if (intExprTest) {
        return dynamic_cast<OpSum*>(&(**intExprTest)) != NULL;
    }
    return false;
}

bool isMatchingIterRef(ExprRef<IntView>& operand, u_int64_t quantId) {
    auto tupleIndexTest = dynamic_cast<OpTupleIndex<IntView>*>(&(*operand));
    if (tupleIndexTest) {
        auto tupleIterTest = dynamic_cast<Iterator<TupleView>*>(
            &(*(tupleIndexTest->tupleOperand)));
        return tupleIterTest && tupleIterTest->id == quantId;
    }
    return false;
}

bool appendLimits(
    pair<ExprRefVec<IntView>, ExprRefVec<IntView>>& lowerAndUpperLimits,
    ExprRef<BoolView>& condition, u_int64_t quantId) {
    auto opLessEqTest = dynamic_cast<OpLessEq*>(&(*condition));
    if (opLessEqTest) {
        if (isMatchingIterRef(opLessEqTest->left, quantId)) {
            lowerAndUpperLimits.second.emplace_back(opLessEqTest->right);
            return true;
        } else if (isMatchingIterRef(opLessEqTest->right, quantId)) {
            lowerAndUpperLimits.first.emplace_back(opLessEqTest->left);
            return true;
        }
        return false;
    }
    auto opLessTest = dynamic_cast<OpLess*>(&(*condition));
    if (opLessTest) {
        if (isMatchingIterRef(opLessTest->left, quantId)) {
            auto val = make<IntValue>();
            val->value = 1;
            auto upperLimit =
                OpMaker<OpMinus>::make(opLessTest->right, val.asExpr());
            lowerAndUpperLimits.second.emplace_back(upperLimit);
            return true;
        } else if (isMatchingIterRef(opLessTest->right, quantId)) {
            auto val = make<IntValue>();
            val->value = 1;
            auto lowerLimit = OpMaker<OpSum>::make(OpMaker<OpSequenceLit>::make(
                ExprRefVec<IntView>({opLessTest->left, val.asExpr()})));
            lowerAndUpperLimits.first.emplace_back(lowerLimit);
            return true;
        }
        return false;
    }
    return false;
}
pair<ExprRefVec<IntView>, ExprRefVec<IntView>> collectLowerAndUpperLimits(
    OpSequenceLit& opProdOperands, u_int64_t quantId) {
    pair<ExprRefVec<IntView>, ExprRefVec<IntView>> lowerAndUpperLimits;
    auto& members = opProdOperands.getMembers<IntView>();
    size_t i = 0;
    while (i < members.size()) {
        auto& operand = members[i];
        auto opToIntTest = dynamic_cast<OpToInt*>(&(*operand));
        if (opToIntTest &&
            appendLimits(lowerAndUpperLimits, opToIntTest->operand, quantId)) {
            opProdOperands.removeMember<IntView>(i);
        } else {
            i++;
        }
    }
    return lowerAndUpperLimits;
}

template <typename Quantifier>
bool optimiseIfOpSumParentWithZeroingCondition(Quantifier& quant,
                                               ExprRef<IntView>& expr,
                                               PathExtension& path) {
    if (path.parent == NULL || !isOpSum(path.parent->expr)) {
        return false;
    }
    IntRange* intRangeTest = dynamic_cast<IntRange*>(&(*(quant.container)));
    if (!intRangeTest) {
        return false;
    }
    auto opProdTest = dynamic_cast<OpProd*>(&(*expr));
    if (!opProdTest) {
        return false;
    }
    auto opProdOperands =
        dynamic_cast<OpSequenceLit*>(&(*(opProdTest->operand)));
    if (!opProdOperands) {
        return false;
    }
    auto lowerAndUpperLimits =
        collectLowerAndUpperLimits(*opProdOperands, quant.quantId);
    bool changeMade = false;
    if (!lowerAndUpperLimits.second.empty()) {
        lowerAndUpperLimits.second.emplace_back(intRangeTest->right);
        intRangeTest->right = OpMaker<OpMin>::make(
            OpMaker<OpSequenceLit>::make(move(lowerAndUpperLimits.second)));
        changeMade = true;
    }
    if (!lowerAndUpperLimits.first.empty()) {
        lowerAndUpperLimits.first.emplace_back(intRangeTest->left);
        intRangeTest->left = OpMaker<OpMax>::make(
            OpMaker<OpSequenceLit>::make(move(lowerAndUpperLimits.first)));
        changeMade = true;
    }
    return true;
}

bool appendLimitsFromOpAndCondition(
    pair<ExprRefVec<IntView>, ExprRefVec<IntView>>& lowerAndUpperLimits,
    ExprRef<BoolView>& condition, u_int64_t quantId) {
    auto opAndTest = dynamic_cast<OpAnd*>(&(*condition));
    if (!opAndTest) {
        return false;
    }
    auto opSequenceLitTest =
        dynamic_cast<OpSequenceLit*>(&(*(opAndTest->operand)));
    if (!opSequenceLitTest) {
        return false;
    }
    for (auto& member : opSequenceLitTest->template getMembers<BoolView>()) {
        if (!appendLimits(lowerAndUpperLimits, member, quantId)) {
            return false;
        }
    }
    return true;
}

template <typename Quantifier>
bool optimiseIfIntRangeWithConditions(Quantifier& quant) {
    IntRange* intRangeTest = dynamic_cast<IntRange*>(&(*(quant.container)));
    if (!intRangeTest) {
        return false;
    }
    if (!quant.condition) {
        return false;
    }
    pair<ExprRefVec<IntView>, ExprRefVec<IntView>> lowerAndUpperLimits;
    if (!appendLimits(lowerAndUpperLimits, quant.condition, quant.quantId) &&
        !appendLimitsFromOpAndCondition(lowerAndUpperLimits, quant.condition,
                                        quant.quantId)) {
        return false;
    }
    quant.condition = nullptr;
    bool changeMade = false;
    if (!lowerAndUpperLimits.second.empty()) {
        lowerAndUpperLimits.second.emplace_back(intRangeTest->right);
        intRangeTest->right = OpMaker<OpMin>::make(
            OpMaker<OpSequenceLit>::make(move(lowerAndUpperLimits.second)));
        changeMade = true;
    }
    if (!lowerAndUpperLimits.first.empty()) {
        lowerAndUpperLimits.first.emplace_back(intRangeTest->left);
        intRangeTest->left = OpMaker<OpMax>::make(
            OpMaker<OpSequenceLit>::make(move(lowerAndUpperLimits.first)));
        changeMade = true;
    }
    return true;
}

template <>
struct ContainerTrigger<SetView> : public SetTrigger, public DelayedTrigger {
    Quantifier<SetView>* op;

    ContainerTrigger(Quantifier<SetView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                typedef ExprRefVec<viewType(members)> VecType;
                if (!mpark::get_if<VecType>(&(op->valuesToUnroll))) {
                    op->valuesToUnroll.emplace<VecType>();
                }
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
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                for (auto& value : vToUnroll) {
                    op->unroll(op->numberElements(), value);
                }
                vToUnroll.clear();
            },
            op->valuesToUnroll);
        deleteTrigger(op->containerDelayedTrigger);
        op->containerDelayedTrigger = nullptr;
    }

    void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<viewType(vToUnroll)>>(member));
                if (!op->containerDelayedTrigger) {
                    op->containerDelayedTrigger =
                        make_shared<ContainerTrigger<SetView>>(op);
                    addDelayedTrigger(op->containerDelayedTrigger);
                }
            },
            op->valuesToUnroll);
    }

    void possibleMemberValueChange(UInt) final {}
    void memberValueChanged(UInt) final{};
    void possibleMemberValuesChange(const std::vector<UInt>&) final {}
    void memberValuesChanged(const std::vector<UInt>&) final{};

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
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<SetView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }

    void hasBecomeUndefined() {
        op->containerDefined = false;
        if (op->numberUndefined == 0) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers,true);
        }
    }
    void hasBecomeDefined() {
        op->containerDefined = true;
        this->valueChanged();
    }
};

template <>
struct InitialUnroller<SetView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier) {
        mpark::visit(
            [&](auto& membersImpl) {
                quantifier.valuesToUnroll
                    .template emplace<BaseType<decltype(membersImpl)>>();
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

    ContainerTrigger(Quantifier<MSetView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                typedef ExprRefVec<viewType(members)> VecType;
                if (!mpark::get_if<VecType>(&(op->valuesToUnroll))) {
                    op->valuesToUnroll.emplace<VecType>();
                }
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
                if (!op->containerDelayedTrigger) {
                    op->containerDelayedTrigger =
                        make_shared<ContainerTrigger<MSetView>>(op);
                    addDelayedTrigger(op->containerDelayedTrigger);
                }
            },
            op->valuesToUnroll);
    }

    void possibleMemberValueChange(UInt) final {}
    void memberValueChanged(UInt) final{};
    void possibleMemberValuesChange(const std::vector<UInt>&) final {}
    void memberValuesChanged(const std::vector<UInt>&) final{};
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
            op->valuesToUnroll);
        deleteTrigger(op->containerDelayedTrigger);
        op->containerDelayedTrigger = nullptr;
    }
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<MSetView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }
    void hasBecomeUndefined() {
        op->containerDefined = false;
        if (op->numberUndefined == 0) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers, true);
        }
    }
    void hasBecomeDefined() {
        op->containerDefined = true;
        this->valueChanged();
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

template <>
struct ContainerTrigger<SequenceView> : public SequenceTrigger,
                                        public DelayedTrigger {
    Quantifier<SequenceView>* op;
    vector<UInt> indicesOfValuesToUnroll;

    ContainerTrigger(Quantifier<SequenceView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                typedef ExprRefVec<viewType(members)> VecType;
                if (!mpark::get_if<VecType>(&(op->valuesToUnroll))) {
                    op->valuesToUnroll.emplace<VecType>();
                }
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
                if (!op->containerDelayedTrigger) {
                    op->containerDelayedTrigger =
                        make_shared<ContainerTrigger<SequenceView>>(op);
                    addDelayedTrigger(op->containerDelayedTrigger);
                }

            },
            op->valuesToUnroll);
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
                    op->valuesToUnroll);
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
            op->valuesToUnroll);
        deleteTrigger(op->containerDelayedTrigger);
        op->containerDelayedTrigger = nullptr;
    }
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<SequenceView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }

    void correctUnrolledTupleIndices(size_t startIndex) {
        debug_log("correct tuple indices from " << startIndex << " onwards.");
        for (size_t i = startIndex; i < op->unrolledIterVals.size(); i++) {
            correctUnrolledTupleIndex(i);
        }
    }
    void correctUnrolledTupleIndex(size_t index) {
        debug_log("Correcting tuple index " << index);
        auto& tuple =
            mpark::get<IterRef<TupleView>>(op->unrolledIterVals[index]);
        auto& intView = mpark::get<ExprRef<IntView>>(tuple->view().members[0]);
        auto& intVal = static_cast<IntValue&>(*intView);
        intVal.changeValue([&]() {
            intVal.value = index + 1;
            return true;
        });
    }
    void hasBecomeUndefined() {
        op->containerDefined = false;
        if (op->numberUndefined == 0) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers,true);
        }
    }
    void hasBecomeDefined() {
        op->containerDefined = true;
        this->valueChanged();
    }

    void memberHasBecomeUndefined(UInt) final {}
    void memberHasBecomeDefined(UInt) final {}
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

template <>
struct InitialUnroller<FunctionView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier) {
        mpark::visit(
            [&](auto& membersImpl) {
                quantifier.valuesToUnroll
                    .template emplace<BaseType<decltype(membersImpl)>>();

                for (size_t i = 0; i < membersImpl.size(); i++) {
                    if (quantifier.container->view().dimensions.size() == 1) {
                        auto tupleFirstMember =
                            quantifier.container->view()
                                .template indexToDomain<IntView>(i);
                        auto unrolledExpr = OpMaker<OpTupleLit>::make(
                            {tupleFirstMember, membersImpl[i]});
                        quantifier.unroll(i, unrolledExpr);
                    } else {
                        auto tupleFirstMember =
                            quantifier.container->view()
                                .template indexToDomain<TupleView>(i);
                        auto unrolledExpr = OpMaker<OpTupleLit>::make(
                            {tupleFirstMember, membersImpl[i]});
                        quantifier.unroll(i, unrolledExpr);
                    }
                }
            },
            quantifier.container->view().range);
    }
};

template <>
struct ContainerTrigger<FunctionView> : public FunctionTrigger {
    Quantifier<FunctionView>* op;

    ContainerTrigger(Quantifier<FunctionView>* op) : op(op) {}
    void possibleImageChange(UInt) final {}
    void imageChanged(UInt) final {}
    void possibleImageChange(const std::vector<UInt>&) final {}
    void imageChanged(const std::vector<UInt>&) final {}
    void possibleValueChange() final {}
    void valueChanged() {
        while (op->numberElements() != 0) {
            op->roll(op->numberElements() - 1);
        }
        InitialUnroller<FunctionView>::initialUnroll(*op);
    }

    void imageSwap(UInt index1, UInt index2) final {
        op->swap(index1, index2, true);
        correctUnrolledTupleIndex(index1);
        correctUnrolledTupleIndex(index2);
    }

    void correctUnrolledTupleIndex(size_t index) {
        debug_log("Correcting tuple index " << index);
        auto& tuple =
            mpark::get<IterRef<TupleView>>(op->unrolledIterVals[index]);
        if (op->container->view().dimensions.size() == 1) {
            auto& preImage =
                mpark::get<ExprRef<IntView>>(tuple->view().members[0]);
            op->container->view().indexToDomain(index, preImage->view());
        } else {
            auto& preImage =
                mpark::get<ExprRef<TupleView>>(tuple->view().members[0]);
            op->container->view().indexToDomain(index, preImage->view());
        }
    }
    void hasBecomeUndefined() {
        op->containerDefined = false;
        if (op->numberUndefined == 0) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers,true);
        }
    }
    void hasBecomeDefined() {
        op->containerDefined = true;
        this->valueChanged();
    }
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<FunctionView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }
};

template struct Quantifier<SetView>;
template struct Quantifier<MSetView>;
template struct Quantifier<SequenceView>;
template struct Quantifier<FunctionView>;
