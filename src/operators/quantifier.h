
#ifndef SRC_OPERATORS_QUANTIFIER_H_
#define SRC_OPERATORS_QUANTIFIER_H_
#include <map>
#include "operators/operatorBase.h"
#include "operators/quantifierBase.h"
#include "types/bool.h"
#include "types/typeOperations.h"
#include "utils/fastIterableIntSet.h"

template <typename ContainerType, typename ContainerValueType,
          typename ReturnType, typename ReturnValueType>
struct Quantifier {
    const int quantId;
    ContainerType container;
    ReturnType expr = ValRef<ReturnValueType>(nullptr);
    std::vector<std::pair<ReturnType, AnyIterRef>> unrolledExprs;
    std::map<u_int64_t, size_t> valueExprMap;
    inline static u_int64_t nextQuantId() {
        static u_int64_t quantId = 0;
        return quantId++;
    }

    Quantifier(ContainerType container, const int id = nextQuantId())
        : quantId(id), container(std::move(container)) {}

    inline void setExpression(ReturnType exprIn) { expr = std::move(exprIn); }

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    inline void hasConsistentState() {
        u_int64_t hash;
        size_t index;
        bool indexFound = true;
        bool success = true;
        if (unrolledExprs.size() != valueExprMap.size()) {
            success = false;
        } else {
            for (size_t i = 0; i < unrolledExprs.size(); ++i) {
                hash = getValueHash(unrolledExprs[i].second);
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
            for (auto& kv : unrolledExprs) {
                std::cerr << kv.second << ",";
            }
            std::cerr << std::endl;
            std::cerr << "Explanation: ";
            if (unrolledExprs.size() != valueExprMap.size()) {
                std::cerr << "\nvalueExprMap has size " << valueExprMap.size()
                          << " but unrolledExprs has size "
                          << unrolledExprs.size() << std::endl;
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

    inline std::pair<size_t, ReturnType&> unroll(
        const AnyValRef& newValue, const bool startTriggeringExpr = true,
        bool evaluateFreshExpr = false) {
        debug_code(this->hasConsistentState());
        mpark::visit(
            [&](auto& newValImpl) {
                typedef typename BaseType<decltype(newValImpl)>::element_type
                    IterValueType;
                auto iterRef = this->newIterRef<IterValueType>();
                evaluateFreshExpr |= unrolledExprs.size() == 0;
                auto& exprToCopy =
                    (evaluateFreshExpr) ? expr : unrolledExprs.back().first;
                unrolledExprs.emplace_back(
                    deepCopyForUnroll(exprToCopy, iterRef), iterRef);
                iterRef.getIterator().attachValue(newValImpl, [&]() {
                    if (evaluateFreshExpr) {
                        evaluate(unrolledExprs.back().first);
                    }
                    if (startTriggeringExpr) {
                        startTriggering(unrolledExprs.back().first);
                    }

                });
                u_int64_t hash = getValueHash(*newValImpl);
                assert(!valueExprMap.count(hash));
                valueExprMap.emplace(hash, unrolledExprs.size() - 1);
            },
            newValue);
        debug_code(this->hasConsistentState());
        return std::pair<size_t, ReturnType&>(unrolledExprs.size() - 1,
                                              unrolledExprs.back().first);
    }

    inline std::pair<size_t, ReturnType> roll(const AnyValRef& val) {
        debug_code(this->hasConsistentState());
        u_int64_t hash = getValueHash(val);

        assert(valueExprMap.count(hash));
        size_t index = valueExprMap[hash];
        std::pair<size_t, ReturnType> removedExpr =
            std::make_pair(index, std::move(unrolledExprs[index].first));
        unrolledExprs[index] = std::move(unrolledExprs.back());
        unrolledExprs.pop_back();
        valueExprMap.erase(hash);
        if (index < unrolledExprs.size()) {
            valueExprMap[getValueHash(unrolledExprs[index].second)] = index;
        }
        debug_code(this->hasConsistentState());
        return removedExpr;
    }

    void valueChanged(u_int64_t oldHash, u_int64_t newHash) {
        if (oldHash == newHash) {
            return;
        }

        assert(valueExprMap.count(oldHash));
        assert(!valueExprMap.count(newHash));
        valueExprMap[newHash] = std::move(valueExprMap[oldHash]);
        valueExprMap.erase(oldHash);
        debug_code(this->hasConsistentState());
    }

    Quantifier<ContainerType, ContainerValueType, ReturnType, ReturnValueType>
    deepCopyQuantifierForUnroll(const AnyIterRef& iterator) const {
        const IterRef<ContainerValueType>* containerPtr =
            mpark::get_if<IterRef<ContainerValueType>>(&container);
        const IterRef<ContainerValueType>* iteratorPtr =
            mpark::get_if<IterRef<ContainerValueType>>(&iterator);
        if (containerPtr != NULL && iteratorPtr != NULL &&
            containerPtr->getIterator().id == iteratorPtr->getIterator().id) {
            Quantifier<ContainerType, ContainerValueType, ReturnType,
                       ReturnValueType>
                newQuantifier(deepCopyForUnroll(container, iterator));
            newQuantifier.expr = expr;

            // this is a new container we are now pointing too
            // no need to populate it with copies of the old unrolled
            // exprs
            return newQuantifier;
        }
        Quantifier<ContainerType, ContainerValueType, ReturnType,
                   ReturnValueType>
            newQuantifier(deepCopyForUnroll(container, iterator), quantId);
        newQuantifier.expr = expr;
        for (auto& expr : unrolledExprs) {
            newQuantifier.unrolledExprs.emplace_back(
                deepCopyForUnroll(expr.first, iterator), expr.second);
        }
        return newQuantifier;
    }
};

struct SetTrigger;
template <typename DerivingQuantifierType, typename ContainerType,
          typename ContainerValueType, typename ExprTrigger>
struct BoolQuantifier : public Quantifier<ContainerType, ContainerValueType,
                                          BoolReturning, BoolValue> {
    using QuantBase =
        Quantifier<ContainerType, ContainerValueType, BoolReturning, BoolValue>;
    using QuantBase::unrolledExprs;
    using QuantBase::container;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);
    std::vector<std::shared_ptr<ExprTrigger>> exprTriggers;

    BoolQuantifier(ContainerType container) : QuantBase(std::move(container)) {}
    BoolQuantifier(QuantBase quant, const FastIterableIntSet& violatingOperands)
        : QuantBase(std::move(quant)), violatingOperands(violatingOperands) {}

    inline void attachTriggerToExpr(size_t index) {
        auto trigger = std::make_shared<ExprTrigger>(
            static_cast<DerivingQuantifierType*>(this), index);
        addTrigger(unrolledExprs[index].first, trigger);
        exprTriggers.emplace_back(trigger);
    }

    inline std::pair<size_t, BoolReturning&> unroll(
        const AnyValRef& newValue, const bool startTriggeringExpr = true,
        const bool evaluateFreshExpr = false) {
        auto indexExprPair =
            QuantBase::unroll(newValue, startTriggeringExpr, evaluateFreshExpr);
        if (getView<BoolView>(indexExprPair.second).violation > 0) {
            violatingOperands.insert(indexExprPair.first);
        }
        attachTriggerToExpr(unrolledExprs.size() - 1);
        return indexExprPair;
    }

    inline std::pair<size_t, BoolReturning> roll(const AnyValRef& val) {
        auto indexExprPair = QuantBase::roll(val);
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
        return indexExprPair;
    }

    void startTriggeringBase() {
        auto& op = static_cast<DerivingQuantifierType&>(*this);
        typedef typename decltype(
            op.containerTrigger)::element_type ContainerTrigger;
        op.containerTrigger = std::make_shared<ContainerTrigger>(&op);
        addTrigger(op.container, op.containerTrigger);
        startTriggering(container);
        for (size_t i = 0; i < unrolledExprs.size(); ++i) {
            attachTriggerToExpr(i);
            startTriggering(unrolledExprs[i].first);
        }
    }

    void stopTriggeringBase() {
        auto& op = static_cast<DerivingQuantifierType&>(*this);
        if (op.containerTrigger) {
            deleteTrigger(op.containerTrigger);
            op.containerTrigger = nullptr;
        }
        stopTriggering(container);
        while (!exprTriggers.empty()) {
            deleteTrigger(exprTriggers.back());
            exprTriggers.pop_back();
        }
        for (auto& expr : op.unrolledExprs) {
            stopTriggering(expr.first);
        }
    }
};
#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
