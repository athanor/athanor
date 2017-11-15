
#ifndef SRC_OPERATORS_SETFORALL_H_
#define SRC_OPERATORS_SETFORALL_H_
#include <vector>
#include "operators/operatorBase.h"
#include "operators/quantifierBase.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
struct SetForAll : public BoolView {
    const int quantId = nextQuantId();
    BoolReturning expr;
    std::vector<std::pair<BoolReturning, QuantValue>> unrolledExprs;

    template <typename T>
    inline void setExpression(BoolReturning exprIn) {
        expr = std::move(exprIn);
    }

    template <typename T>
    inline QuantRef<T> newQuantRef() {
        return QuantRef<T>(quantId);
    }

    inline void unroll(const Value& newValue) {
        mpark::visit(
            [&](auto& newValImpl) {
                auto quantRef = newQuantRef<
                    typename BaseType<decltype(newValImpl)>::element_type>();
                unrolledExprs.emplace_back(deepCopyForUnroll(expr, quantRef),
                                           quantRef);
                quantRef.getQuantifier().attachValue(newValImpl);
            },
            newValue);
    }
};

#endif /* SRC_OPERATORS_SETFORALL_H_ */
