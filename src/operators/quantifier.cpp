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
#include "operators/quantifier.h"
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

#define exportOptimiseFunction(name)           \
    template pair<bool, ExprRef<SequenceView>> \
    Quantifier<name##View>::optimise(PathExtension path);

exportOptimiseFunction(Set);
exportOptimiseFunction(MSet);
exportOptimiseFunction(Sequence);
exportOptimiseFunction(Function);
