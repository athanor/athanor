
#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_

#include "operators/operatorBase.h"
#include "operators/quantifierBase.h"

inline static int nextQuantId() {
    static int id = 0;
    return id++;
}

template <typename ContainerType, typename ContainerValueType,
          typename ReturnType, typename ReturnValueType>
struct Quantifier {
    const int quantId;
    ContainerType container;
    ReturnType expr = ValRef<ReturnValueType>(nullptr);
    std::vector<std::pair<ReturnType, IterValue>> unrolledExprs;
    std::unordered_map<u_int64_t, size_t> valueExprMap;

    Quantifier(ContainerType container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}

    inline void setExpression(ReturnType exprIn) { expr = std::move(exprIn); }

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    inline std::pair<size_t, ReturnType&> unroll(const Value& newValue) {
        mpark::visit(
            [&](auto& newValImpl) {
                auto iterRef = newIterRef<
                    typename BaseType<decltype(newValImpl)>::element_type>();
                if (unrolledExprs.size() == 0) {
                    unrolledExprs.emplace_back(deepCopyForUnroll(expr, iterRef),
                                               iterRef);
                } else {
                    unrolledExprs.emplace_back(
                        deepCopyForUnroll(unrolledExprs.back().first, iterRef),
                        iterRef);
                }
                valueExprMap.emplace(getValueHash(newValImpl),
                                     unrolledExprs.size() - 1);
                iterRef.getIterator().attachValue(newValImpl);
            },
            newValue);
        return std::pair<size_t, ReturnType&>(unrolledExprs.size() - 1,
                                              unrolledExprs.back().first);
    }

    inline std::pair<size_t, ReturnType> roll(const Value& val) {
        u_int64_t hash = getValueHash(val);
        assert(valueExprMap.count(hash));
        size_t index = valueExprMap[hash];
        auto removedExpr =
            std::make_pair(index, std::move(unrolledExprs[index].first));
        unrolledExprs[index] = std::move(unrolledExprs.back());
        unrolledExprs.pop_back();
        valueExprMap.erase(hash);
        return removedExpr;
    }

    Quantifier<ContainerType, ContainerValueType, ReturnType, ReturnValueType>
    deepCopyQuantifierForUnroll(const IterValue& iterator) const {
        Quantifier<ContainerType, ContainerValueType, ReturnType,
                   ReturnValueType>
            newQuantifier(deepCopyForUnroll(container, iterator));
        const IterRef<ContainerValueType>* containerPtr =
            mpark::get_if<IterRef<ContainerValueType>>(
                &(newQuantifier.container));
        const IterRef<ContainerValueType>* iteratorPtr =
            mpark::get_if<IterRef<ContainerValueType>>(&iterator);
        if (containerPtr != NULL && iteratorPtr != NULL &&
            containerPtr->getIterator().id == iteratorPtr->getIterator().id) {
            // this is a new container we are now pointing too
            // no need to populate it with copies of the old unrolled exprs
            return newQuantifier;
        }
        newQuantifier.expr = expr;
        for (auto& expr : unrolledExprs) {
            newQuantifier.unrolledExprs.emplace_back(
                deepCopyForUnroll(expr.first, iterator), expr.second);
        }
        return newQuantifier;
    }
};

template <typename ContainerType, typename ContainerValueType,
          typename ExprTrigger, typename ExprUnrollTrigger>
struct BoolQuantifier : public Quantifier<ContainerType, ContainerValueType,
                                          BoolReturning, BoolValue> {
    using Quantifier::Quantifier;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::vector<ExprTrigger> exprTriggers;
    std::vector<ExprUnrollTrigger> exprUnrollTriggers;
    // should be empty if the quantifier expression does not consist of only an
    // IterRef
    bool exprIsIterRef;
    inline void setExpression(ReturnType exprIn) {
        Quantifier::setExpression(std::move(expr));
        exprIsIterRef = (mpark::get_if<IterRef<BoolValue>>(&expr) != NULL);
    }

    inline std::pair<size_t, BoolReturning&> unroll(const Value& newValue) {
        auto indexExprPair = Quantifier::unroll(newValue);
        if (getView<BoolView>(indexExprPair.first).violation > 0) {
            violatingOperands.insert(index);
        }
        return newExpr;
    }

    inline std::pair<size_t, ReturnType> roll(const Value& val) {
        auto indexExprPair = Quantifier::roll(val);
        u_int64_t removedViolation =
            getView<BoolView>(indexExprPair.second).violation;
        if (removedViolation > 0) {
            violatingOperands.erase(indexExprPair.first);
        }
        if (violatingOperands.erase(unrolledExprs.size())) {
            violatingOperands.insert(indexExprPair.first);
        }
        deleteTrigger(exprTriggers[indexExprPair.first]);
        exprTriggers[indexExprPair.first] = std::move(exprTriggers.back());
        exprTriggers.pop_back();
        exprTriggers[indexExprPair.first]->index = indexExprPair.first;
        if (exprIsIterRef) {
            deleteTrigger(exprUnrollTriggers[indexExprPair.first]);
            exprUnrollTriggers[indexExprPair.first] =
                std::move(exprUnrollTriggers.back());
            exprTriggers.pop_back();
            exprTriggers[indexExprPair.first]->index = indexExprPair.first;
        }
        return indexExprPair;
    }
};
#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
