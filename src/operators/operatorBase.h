
#ifndef SRC_OPERATORS_OPERATORBASE_H_
#define SRC_OPERATORS_OPERATORBASE_H_
#include "operators/quantifierBase.h"
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

#endif /* SRC_OPERATORS_OPERATORBASE_H_ */
