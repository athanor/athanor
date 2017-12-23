
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
        debug_log("unrolling " << newValue);
        debug_code(this->hasConsistentState());
        mpark::visit(
            [&](auto& newValImpl) {
                typedef typename BaseType<decltype(newValImpl)>::element_type
                    IterValueType;
                auto iterRef = this->newIterRef<IterValueType>();
                evaluateFreshExpr |= unrolledExprs.size() == 0;
                auto& exprToCopy =
                    (evaluateFreshExpr) ? expr : unrolledExprs.back().first;
                const auto& oldValueOfIter =
                    (evaluateFreshExpr) ? ValRef<IterValueType>(nullptr)
                                        : mpark::get<IterRef<IterValueType>>(
                                              unrolledExprs.back().second)
                                              .getIterator()
                                              .getValue();
                unrolledExprs.emplace_back(
                    deepCopyForUnroll(exprToCopy, iterRef), iterRef);
                iterRef.getIterator().triggerValueChange(
                    oldValueOfIter, newValImpl, [&]() {
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

    inline std::pair<size_t, ReturnType> roll(u_int64_t hash) {
        debug_log("rolling " << hash);
        debug_code(this->hasConsistentState());
        assert(valueExprMap.count(hash));
        size_t index = valueExprMap[hash];
        std::pair<size_t, ReturnType> removedExpr =
            std::make_pair(index, std::move(unrolledExprs[index].first));
        debug_log("hash points to value " << unrolledExprs[index].second);
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
        Quantifier<ContainerType, ContainerValueType, ReturnType,
                   ReturnValueType>
            newQuantifier(deepCopyForUnroll(container, iterator), quantId);
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
        debug_code(hasConsistentState());
        auto indexExprPair =
            QuantBase::unroll(newValue, startTriggeringExpr, evaluateFreshExpr);
        u_int64_t addedViolation =
            getView<BoolView>(indexExprPair.second).violation;
        static_cast<DerivingQuantifierType*>(this)->operandAdded(
            addedViolation);
        if (addedViolation > 0) {
            violatingOperands.insert(indexExprPair.first);
        }
        if (startTriggeringExpr) {
            attachTriggerToExpr(unrolledExprs.size() - 1);
        }
        debug_code(hasConsistentState());
        return indexExprPair;
    }

    inline std::pair<size_t, BoolReturning> roll(u_int64_t hash) {
        debug_code(hasConsistentState());
        auto indexExprPair = QuantBase::roll(hash);
        u_int64_t removedViolation =
            getView<BoolView>(indexExprPair.second).violation;
        static_cast<DerivingQuantifierType*>(this)->operandRemoved(
            removedViolation);
        if (removedViolation > 0) {
            violatingOperands.erase(indexExprPair.first);
        }
        if (violatingOperands.erase(unrolledExprs.size())) {
            violatingOperands.insert(indexExprPair.first);
        }
        if (exprTriggers.size() > 0) {
            deleteTrigger(exprTriggers[indexExprPair.first]);
            exprTriggers[indexExprPair.first] = std::move(exprTriggers.back());
            exprTriggers.pop_back();
            if (indexExprPair.first < exprTriggers.size()) {
                exprTriggers[indexExprPair.first]->index = indexExprPair.first;
            }
        }
        debug_code(hasConsistentState());
        return indexExprPair;
    }

    void startTriggeringBase() {
        debug_code(assert(exprTriggers.size() == 0));
        debug_code(hasConsistentState());
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
        debug_code(hasConsistentState());
    }

    void stopTriggeringBase() {
        debug_code(hasConsistentState());
        auto& op = static_cast<DerivingQuantifierType&>(*this);
        if (op.containerTrigger) {
            deleteTrigger(op.containerTrigger);
            op.containerTrigger = nullptr;
            stopTriggering(container);
        }
        if (exprTriggers.empty()) {
            return;
        }
        while (!exprTriggers.empty()) {
            deleteTrigger(exprTriggers.back());
            exprTriggers.pop_back();
        }
        for (auto& expr : op.unrolledExprs) {
            stopTriggering(expr.first);
        }
        debug_code(hasConsistentState());
    }

    inline void hasConsistentState() {
        if (exprTriggers.size() != 0 &&
            exprTriggers.size() != unrolledExprs.size()) {
            std::cerr << "number unrolled Exprs = " << unrolledExprs.size()
                      << ", number expr triggers = " << exprTriggers.size()
                      << std::endl;
            assert(false);
        }
    }
};
#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
