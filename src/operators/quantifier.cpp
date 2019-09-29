#include "operators/quantifier.h"
#include <vector>
#include "operators/intRange.h"
#include "operators/opAnd.h"
#include "operators/opImplies.h"
#include "operators/opIntEq.h"
#include "operators/opLess.h"
#include "operators/opLessEq.h"
#include "operators/opMinus.h"
#include "operators/opProd.h"
#include "operators/opSequenceIndex.h"
#include "operators/opSequenceLit.h"
#include "operators/opSum.h"
#include "operators/opToInt.h"
#include "operators/opTupleIndex.h"
#include "operators/opTupleLit.h"
#include "operators/operatorMakers.h"
#include "operators/quantifier.h"
#include "triggers/allTriggers.h"
#include "types/allTypes.h"
#include "types/intVal.h"
template <typename View>
struct OpSubstringQuantify;
template <typename View>
struct OpMaker<OpSubstringQuantify<View>> {
    static ExprRef<SequenceView> make(ExprRef<SequenceView> sequence,
                                      ExprRef<IntView> lowerBound,
                                      ExprRef<IntView> upperBound,
                                      size_t windowSize);
};
template <typename View>
struct OpTupleIndex;

using namespace std;
bool isOpSum(const AnyExprRef& expr) {
    auto intExprTest = mpark::get_if<ExprRef<IntView>>(&expr);
    return intExprTest && getAs<OpSum>(*intExprTest);
}

bool isOpAnd(const AnyExprRef& expr) {
    auto boolExprTest = mpark::get_if<ExprRef<BoolView>>(&expr);
    return boolExprTest && getAs<OpAnd>(*boolExprTest);
}

bool isMatchingIterRef(ExprRef<IntView>& operand, UInt64 quantId) {
    auto tupleIndexTest = getAs<OpTupleIndex<IntView>>(operand);
    if (tupleIndexTest) {
        auto tupleIterTest =
            getAs<Iterator<TupleView>>(tupleIndexTest->tupleOperand);
        return tupleIterTest && tupleIterTest->id == quantId;
    }
    return false;
}

// find the first instance of the operator Op, remove it from the sequence and
// return the bool expression which should be a child of the Op
lib::optional<ExprRef<BoolView>> extractBuriedConditionFromOpProd(
    OpSequenceLit& operands) {
    return mpark::visit(
        [&](auto& members) -> lib::optional<ExprRef<BoolView>> {
            for (size_t i = 0; i < members.size(); i++) {
                auto& operand = members[i];
                auto opTest = getAs<OpToInt>(operand);
                if (opTest) {
                    auto condition = opTest->getCondition();
                    members.erase(members.begin() + i);
                    return condition;
                }
            }
            return lib::nullopt;
        },
        operands.members);
}

// if the parent is an OpAnd with an implies in the expression, the implies
// condition can be moved to the quantifier condition. similarly, if the parent
// is an OpSum and there is the typical multiplying  by a toInt(condition), this
// can be moved to the quantifier condition also. there are a few overloads of
// this function below

template <typename Quant>
void addToCondition(Quant& quant, ExprRef<BoolView> newCondition) {
    if (quant.condition) {
        quant.condition = OpMaker<OpAnd>::make(OpMaker<OpSequenceLit>::make(
            ExprRefVec<BoolView>({quant.condition, newCondition})));
    } else {
        quant.condition = newCondition;
    }
}
template <typename Quant>
bool tryPromotingSubExpressionsToConditions(Quant& quant,
                                            ExprRef<IntView>& expr,
                                            PathExtension& path) {
    if (path.isTop() || !isOpSum(path.expr)) {
        return false;
    }
    auto opProdTest = getAs<OpProd>(expr);
    if (!opProdTest) {
        return false;
    }
    auto opProdOperands = getAs<OpSequenceLit>(opProdTest->operand);
    if (!opProdOperands || opProdOperands->numberElements() < 2) {
        return false;
    }
    auto newConditionOption = extractBuriedConditionFromOpProd(*opProdOperands);
    if (newConditionOption) {
        debug_log(
            "Optimise: moving opToInt condition from OpProd to the condition "
            "of "
            "the quantifier");
        addToCondition(quant, *newConditionOption);
        return true;
    } else {
        return false;
    }
}

template <typename Quant>
bool tryPromotingSubExpressionsToConditions(Quant& quant,
                                            ExprRef<BoolView>& expr,
                                            PathExtension& path) {
    if (path.isTop() || !isOpAnd(path.expr)) {
        return false;
    }
    auto opImpliesTest = getAs<OpImplies>(expr);
    if (opImpliesTest) {
        debug_log(
            "Optimise: moving OpImplies condition to the condition of "
            "the quantifier");
        addToCondition(quant, opImpliesTest->left);
        quant.setExpression(opImpliesTest->right);
        return true;
    } else {
        return false;
    }
}

template <typename Quant, typename View>
bool tryPromotingSubExpressionsToConditions(Quant&, ExprRef<View>&,
                                            PathExtension&) {
    return false;
}

bool areAllConstant(const ExprRefVec<IntView>& exprs) {
    for (const auto& expr : exprs) {
        if (!expr->isConstant()) {
            return false;
        }
    }
    return true;
}

bool appendLimits(
    pair<ExprRefVec<IntView>, ExprRefVec<IntView>>& lowerAndUpperLimits,
    ExprRef<BoolView>& condition, UInt64 quantId) {
    auto opLessEqTest = getAs<OpLessEq>(condition);
    if (opLessEqTest) {
        if (isMatchingIterRef(opLessEqTest->left, quantId)) {
            debug_log(
                "Optimise: moving OpLesEq from quantifier condition to "
                "IntRange.");
            lowerAndUpperLimits.second.emplace_back(opLessEqTest->right);
            return true;
        } else if (isMatchingIterRef(opLessEqTest->right, quantId)) {
            debug_log(
                "Optimise: moving OpLesEq from quantifier condition to "
                "IntRange.");
            lowerAndUpperLimits.first.emplace_back(opLessEqTest->left);
            return true;
        }
        return false;
    }
    auto opLessTest = getAs<OpLess>(condition);
    if (opLessTest) {
        if (isMatchingIterRef(opLessTest->left, quantId)) {
            debug_log(
                "Optimise: moving OpLes from quantifier condition to "
                "IntRange.");
            auto val = make<IntValue>();
            val->value = 1;
            auto upperLimit =
                OpMaker<OpMinus>::make(opLessTest->right, val.asExpr());
            lowerAndUpperLimits.second.emplace_back(upperLimit);
            return true;
        } else if (isMatchingIterRef(opLessTest->right, quantId)) {
            debug_log(
                "Optimise: moving OpLes from quantifier condition to "
                "IntRange.");
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

bool appendLimitsFromOpAndCondition(
    pair<ExprRefVec<IntView>, ExprRefVec<IntView>>& lowerAndUpperLimits,
    ExprRef<BoolView>& condition, UInt64 quantId,
    size_t& numberOpAndOperandsLeft) {
    auto opAndTest = getAs<OpAnd>(condition);
    if (!opAndTest) {
        return false;
    }
    auto opSequenceLitTest = getAs<OpSequenceLit>(opAndTest->operand);
    if (!opSequenceLitTest) {
        return false;
    }
    auto& members = opSequenceLitTest->template getMembers<BoolView>();
    numberOpAndOperandsLeft = members.size();
    bool success = false;
    size_t i = 0;
    while (i < members.size()) {
        if (appendLimits(lowerAndUpperLimits, members[i], quantId)) {
            --numberOpAndOperandsLeft;
            success = true;
            opSequenceLitTest->removeMember<BoolView>(i);
        } else {
            ++i;
        }
    }
    return success;
}

template <typename Quantifier>
bool optimiseIfIntRangeWithConditions(Quantifier& quant) {
    auto intRangeTest = getAs<IntRange>(quant.container);
    if (!intRangeTest) {
        return false;
    }
    if (!quant.condition) {
        return false;
    }
    pair<ExprRefVec<IntView>, ExprRefVec<IntView>> lowerAndUpperLimits;
    size_t numberOpAndOperandsLeft = 0;
    if (!appendLimits(lowerAndUpperLimits, quant.condition, quant.quantId) &&
        !appendLimitsFromOpAndCondition(lowerAndUpperLimits, quant.condition,
                                        quant.quantId,
                                        numberOpAndOperandsLeft)) {
        return false;
    }
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
    if (numberOpAndOperandsLeft == 0) {
        debug_log("Optimise: no more conditions left, removing condition.");
        quant.condition = nullptr;
    }
    return true;
}

template <typename View>
bool optimiseIfCanBeConvertedToSubstringQuantifier(Quantifier<View>&) {
    return false;
}

template <>
bool optimiseIfCanBeConvertedToSubstringQuantifier<SequenceView>(
    Quantifier<SequenceView>& quant);

template <typename ContainerType>
pair<bool, ExprRef<SequenceView>> Quantifier<ContainerType>::optimiseImpl(
    ExprRef<SequenceView>&, PathExtension path) {
    bool optimised = false;
    auto newOp = make_shared<Quantifier<ContainerType>>(*this);
    auto newOpAsExpr = ExprRef<SequenceView>(newOp);
    optimised |= optimise(newOpAsExpr, newOp->container, path);
    if (newOp->condition) {
        optimised |= optimise(newOpAsExpr, newOp->condition, path, true);
    }

    mpark::visit(
        [&](auto& expr) {
            optimised |= optimise(newOpAsExpr, expr, path);
            optimised |=
                tryPromotingSubExpressionsToConditions(*newOp, expr, path);
        },
        newOp->expr);
    optimised |= optimiseIfIntRangeWithConditions(*newOp);
optimised |= optimiseIfCanBeConvertedToSubstringQuantifier(*newOp);
    return make_pair(optimised, newOpAsExpr);
}

#define exportOptimiseFunction(name)                             \
    template pair<bool, ExprRef<SequenceView>>                   \
    Quantifier<name##View>::optimiseImpl(ExprRef<SequenceView>&, \
                                         PathExtension path);

exportOptimiseFunction(Set);
exportOptimiseFunction(MSet);
exportOptimiseFunction(Sequence);
exportOptimiseFunction(Function);
