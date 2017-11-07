
#ifndef SRC_OPERATORS_OPERATORBASE_H_
#define SRC_OPERATORS_OPERATORBASE_H_
#include "operators/quantifierBase.h"
#include "search/violationDescription.h"

// overload for function deepCopyForUnrolling for all base operator variant
// types i.e. BoolReturning, IntReturning, SetReturning, etc.
template <typename ReturnType>
inline ReturnType deepCopyForUnroll(const ReturnType& expr,
                                    const QuantValue& unrollingQuantifier) {
    return mpark::visit(
        [&](auto& exprImpl) -> ReturnType {
            return deepCopyForUnrollingImpl(exprImpl, unrollingQuantifier);
        },
        expr);
}

/// overload of deepCopyForUnroll that catches operators
template <typename T>
std::shared_ptr<T> deepCopyForUnrollOverload(
    const std::shared_ptr<T>& ptr, const QuantValue& unrollingQuantifier) {
    return deepCopyForUnroll(*ptr, unrollingQuantifier);
}

// ValRef overload for deepCopyOverload, note that ValRefs are not supposed to
// be deepCopied when unrolling
template <typename T>
inline ValRef<T> deepCopyForUnrollOverload(const ValRef<T>& val,
                                           const QuantValue&) {
    return val;
}

template <typename View, typename Operator>
inline View& getView(const Operator& op) {
    return mpark::visit(
        [&](auto& opImpl) -> View& { return reinterpret_cast<View&>(*opImpl); },
        op);
}

template <typename Operator>
inline void evaluate(Operator& op) {
    mpark::visit([&](auto& opImpl) { evaluate(*opImpl); }, op);
}

template <typename Operator>
inline void startTriggering(Operator& op) {
    mpark::visit([&](auto& opImpl) { startTriggering(*opImpl); }, op);
}

template <typename Operator>
inline void stopTriggering(Operator& op) {
    mpark::visit([&](auto& opImpl) { stopTriggering(*opImpl); }, op);
}

template <typename Operator>
inline void updateViolationDescription(
    const Operator& op, u_int64_t parentViolation,
    ViolationDescription& violationDescription) {
    mpark::visit(
        [&](auto& opImpl) {
            updateViolationDescription(*opImpl, parentViolation,
                                       violationDescription);
        },
        op);
}

#endif /* SRC_OPERATORS_OPERATORBASE_H_ */
