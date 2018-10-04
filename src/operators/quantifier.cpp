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
#include "operators/quantifier.hpp"
#include "triggers/allTriggers.h"
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
    bool containerOptimised = optimiseIfIntRangeWithConditions(*this);
    changeMade |= containerOptimised;

    mpark::visit(overloaded(
                     [&](ExprRef<IntView>& expr) {
                         auto optResult = expr->optimise(path.extend(expr));
                         changeMade |= optResult.first;
                         expr = optResult.second;
                         containerOptimised |=
                             optimiseIfOpSumParentWithZeroingCondition(
                                 *this, expr, path);
                         changeMade |= containerOptimised;
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
    if (changeMade) {
        container->optimise(path.extend(container));
        mpark::visit([&](auto& expr) { expr->optimise(path.extend(expr)); },
                     expr);
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

bool areAllConstant(const ExprRefVec<IntView>& exprs) {
    for (const auto& expr : exprs) {
        if (!expr->isConstant()) {
            return false;
        }
    }
    return true;
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
        quant.container->setConstant(
            quant.container->isConstant() &&
            areAllConstant(lowerAndUpperLimits.second));
        lowerAndUpperLimits.second.emplace_back(intRangeTest->right);
        intRangeTest->right = OpMaker<OpMin>::make(
            OpMaker<OpSequenceLit>::make(move(lowerAndUpperLimits.second)));
        changeMade = true;
    }
    if (!lowerAndUpperLimits.first.empty()) {
        quant.container->setConstant(quant.container->isConstant() &&
                                     areAllConstant(lowerAndUpperLimits.first));
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
        quant.container->setConstant(
            quant.container->isConstant() &&
            areAllConstant(lowerAndUpperLimits.second));
        lowerAndUpperLimits.second.emplace_back(intRangeTest->right);
        intRangeTest->right = OpMaker<OpMin>::make(
            OpMaker<OpSequenceLit>::make(move(lowerAndUpperLimits.second)));
        changeMade = true;
    }
    if (!lowerAndUpperLimits.first.empty()) {
        quant.container->setConstant(quant.container->isConstant() &&
                                     areAllConstant(lowerAndUpperLimits.first));
        lowerAndUpperLimits.first.emplace_back(intRangeTest->left);
        intRangeTest->left = OpMaker<OpMax>::make(
            OpMaker<OpSequenceLit>::make(move(lowerAndUpperLimits.first)));
        changeMade = true;
    }
    return true;
}

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
            op->swapExprs(indexOfRemovedValue, op->numberElements() - 1);
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

    void memberValueChanged(UInt) final{};
    void memberValuesChanged(const std::vector<UInt>&) final{};

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

    ContainerTrigger(Quantifier<SequenceView>* op) : op(op) {
        mpark::visit(
            [&](auto& members) {
                typedef ExprRefVec<viewType(members)> VecType;
                if (!mpark::get_if<VecType>(&(op->valuesToUnroll))) {
                    op->valuesToUnroll.emplace<VecType>();
                    op->indicesOfValuesToUnroll.clear();
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
                op->indicesOfValuesToUnroll.emplace_back(index);
                if (!op->containerDelayedTrigger) {
                    op->containerDelayedTrigger =
                        make_shared<ContainerTrigger<SequenceView>>(op);
                    addDelayedTrigger(op->containerDelayedTrigger);
                }
            },
            op->valuesToUnroll);
    }

    void subsequenceChanged(UInt, UInt) final{};

    void valueChanged() {
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1,
                               ExprRef<BoolView>(nullptr));
        }
        op->indicesOfValuesToUnroll.clear();
        mpark::visit(
            [&](auto& membersImpl) {
                auto& vToUnroll = mpark::get<ExprRefVec<viewType(membersImpl)>>(
                    op->valuesToUnroll);
                op->indicesOfValuesToUnroll.clear();
                vToUnroll.clear();
                for (auto& member : membersImpl) {
                    this->valueAdded(vToUnroll.size(), member);
                }
            },
            op->container->view().members);
    }

    void positionsSwapped(UInt index1, UInt index2) final {
        op->swapExprs(index1, index2);
        correctUnrolledTupleIndex(index1);
        correctUnrolledTupleIndex(index2);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                debug_code(assert(vToUnroll.size() ==
                                  op->indicesOfValuesToUnroll.size()));
                const UInt MAX_UINT = ~((UInt)0);
                UInt minIndex = MAX_UINT;
                for (size_t i = 0; i < vToUnroll.size(); i++) {
                    auto tupleFirstMember = make<IntValue>();
                    tupleFirstMember->value =
                        op->indicesOfValuesToUnroll[i] + 1;
                    auto unrolledExpr = OpMaker<OpTupleLit>::make(
                        {tupleFirstMember.asExpr(), vToUnroll[i]});
                    op->unroll(op->indicesOfValuesToUnroll[i], unrolledExpr);
                    if (op->indicesOfValuesToUnroll[i] + 1 <
                            op->unrolledIterVals.size() &&
                        op->indicesOfValuesToUnroll[i] + 1 < minIndex) {
                        minIndex = op->indicesOfValuesToUnroll[i] + 1;
                    }
                }
                vToUnroll.clear();
                op->indicesOfValuesToUnroll.clear();
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
                          op->triggers, true);
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

    void imageChanged(UInt) final {}

    void imageChanged(const std::vector<UInt>&) final {}

    void valueChanged() {
        while (op->numberElements() != 0) {
            op->roll(op->numberElements() - 1);
        }
        InitialUnroller<FunctionView>::initialUnroll(*op);
    }

    void imageSwap(UInt index1, UInt index2) final {
        op->swapExprs(index1, index2);
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
                          op->triggers, true);
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
