
#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
/*
#include <map>
#include "operators/iterator.h"
#include "operators/operatorBase.h"
#include "operators/quantifierBase.h"
#include "types/bool.h"
#include "types/typeOperations.h"
#include "utils/fastIterableIntSet.h"

template <typename ContainerType, typename ExprType>
struct Quantifier : public QuantifierView<ExprType> {
    typedef
        typename AssociatedValueType<ContainerType>::type ContainerValueType;
    const int quantId;
    ContainerType container;
    ExprType expr = ValRef<ReturnValueType>(nullptr);

    inline static u_int64_t nextQuantId() {
        static u_int64_t quantId = 0;
        return quantId++;
    }

    Quantifier(ContainerType container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}

    inline void setExpression(ExprType exprIn) { expr = std::move(exprIn); }

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    inline void hasConsistentState() {
        u_int64_t hash;
        size_t index;
        bool indexFound = true;
        bool success = true;
        if (numberExprs() != valueExprMap.size()) {
            success = false;
        } else {
            for (size_t i = 0; i < numberExprs(); ++i) {
                hash = getValueHash(iterValue(i));
                index = i;
                auto iter = valueExprMap.find(hash);
                if (iter == valueExprMap.end()) {
                    indexFound = false;
                    success = false;
                    break;
                } else if (iter->second != index) {
                    success = false;
                    break;
                }
            }
        }
        if (!success) {
            std::cerr << "Error: inconsistent state detected.\nFunction: "
                      << __func__ << " file: " << __FILE__
                      << ", line: " << __LINE__
                      << "\nvalueExprMap: " << valueExprMap
                      << "\nunrolledExprs: ";
            for (size_t i = 0; i < numberExprs(); i++) {
                std::cerr << iterValue(i) << ",";
            }
            std::cerr << std::endl;
            std::cerr << "Explanation: ";
            if (numberExprs() != valueExprMap.size()) {
                std::cerr << "\nvalueExprMap has size " << valueExprMap.size()
                          << " but unrolledExprs has size " << numberExprs()
                          << std::endl;
            } else if (!indexFound) {
                std::cerr << "\nkey " << hash << " for index " << index
                          << " was not found.\n";
            } else {
                std::cerr << "key " << hash << " maps to " << valueExprMap[hash]
                          << " but it was instead found in position " << index
                          << std::endl;
            }
            abort();
        }
    }

    inline std::pair<size_t, ExprType&> unroll(
        const AnyValRef& newValue, const bool startTriggeringExpr = true,
        bool evaluateFreshExpr = false) {
        debug_log("unrolling " << newValue);
        debug_code(this->hasConsistentState());

        u_int64_t hash = getValueHash(*newValImpl);

        if (valueExprMap.count(hash)) {
            debug_log("Value already here, increasing count");
            size_t index = valueExprMap[hash];
            increaseExprCount(index);
            return std::make_pair<size_t, ExprType&>(index, expr(index));
        }

        mpark::visit(
            [&](auto& newValImpl) {
                typedef typename BaseType<decltype(newValImpl)>::element_type
                    IterValueType;
                auto iterRef = this->newIterRef<IterValueType>();
                evaluateFreshExpr |= numberExprs() == 0;
                auto& exprToCopy = (evaluateFreshExpr) ? expr : lastExpr();
                const auto& oldValueOfIter =
                    (evaluateFreshExpr)
                        ? ValRef<IterValueType>(nullptr)
                        : mpark::get<IterRef<IterValueType>>(lastIterValue())
                              .getIterator()
                              .getValue();
                addUnrolledExpr(deepCopyForUnroll(exprToCopy, iterRef),
                                iterRef);
                iterRef.getIterator().triggerValueChange(
                    oldValueOfIter, newValImpl, [&]() {
                        if (evaluateFreshExpr) {
                            evaluate(lastExpr());
                        }
                        if (startTriggeringExpr) {
                            startTriggering(lastExpr());
                        }
                    });
                u_int64_t hash = getValueHash(*newValImpl);
                valueExprMap.emplace(hash, numberExprs() - 1);
            },
            newValue);
        debug_code(this->hasConsistentState());
        return std::pair<size_t, ExprType&>(numberExprs() - 1, lastExpr());
    }

    inline void roll(u_int64_t hash) {
        debug_log("rolling " << hash);
        debug_code(this->hasConsistentState());
        debug_log("hash points to value " << iterValue(index));

        size_t index = valueExprMap[hash];
        if (decreaseExprCount(index) != 0) {
            return;
        }
        valueExprMap.erase(hash);
        if (index < numberExprs()) {
            valueExprMap[getValueHash(iterValue(index))] = index;
        }
        debug_code(this->hasConsistentState());
    }

    void valueChanged(u_int64_t oldHash, u_int64_t newHash) {
        if (oldHash == newHash) {
            return;
        }
        assert(valueExprMap.count(oldHash));
        size_t oldIndex = valueExprMap[oldHash];
        bool newExprExists = valueExprMap[newHash];
        if (exprRepetitionCount(oldIndex) == 1) {
            if (newExprExists) {
                increaseExprCount(valueExprMap[newHash]);
                decreaseExprCount(oldIndex);
            } else {
                valueExprMap[newHash] = valueExprMap[oldHash];
            }
                valueExprMap.erase(oldHash);
        } else {
            //old expr repetition count is > 0
            decreaseExprCount(oldIndex);
            if (newExprExists) {
                increaseExprCount(valueExprMap[newHash]);
            } else {

            }
        }

        if (valueExprMap.count(newHash)) {
            increaseExprCount(valueExprMap[hash]);
            return;
        }
        valueExprMap[newHash] = debug_code(this->hasConsistentState());
    }

    Quantifier<ContainerType, ExprType> deepCopyQuantifierForUnroll(
        const AnyIterRef& iterator) const {
        Quantifier<ContainerType, ExprType> newQuantifier(
            deepCopyForUnroll(container, iterator), quantId);
        newQuantifier.expr = deepCopyForUnroll(expr, iterator);
        // we want to identify if the unrolling value is the our
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
        newQuantifier.valueExprMap = valueExprMap;
        for (auto& expr : unrolledExprs) {
            newQuantifier.unrolledExprs.emplace_back(
                deepCopyForUnroll(expr.first, iterator), expr.second);
        }
        return newQuantifier;
    }
};
*/
#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
