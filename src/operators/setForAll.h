
#ifndef SRC_OPERATORS_SETFORALL_H_
#define SRC_OPERATORS_SETFORALL_H_
#include <vector>
#include "operators/operatorBase.h"
#include "operators/quantifierBase.h"
#include "types/bool.h"
#include "utils/fastIterableIntSet.h"
struct SetForAll : public BoolView {
    const int quantId = nextQuantId();
    std::vector<BoolReturning> unrolled;
    std::vector<QuantValue> quantRefs;
    int numberUnrolled = 0;
    template <typename T>
    inline void setExpression(const QuantRef<T>& quantifier,
                              BoolReturning expr) {
        numberUnrolled = 0;
        unrolled.clear();
        quantRefs.clear();
        unrolled.emplace_back(std::move(expr));
        quantRefs.emplace_back(quantifier);
    }

    template <typename T>
    inline QuantRef<T> newQuantRef() {
        return QuantRef<T>(quantId);
    }

   private:
    void unroll(const Value&) {}
};

#endif /* SRC_OPERATORS_SETFORALL_H_ */
