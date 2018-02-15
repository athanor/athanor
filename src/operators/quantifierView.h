
#ifndef SRC_OPERATORS_QUANTIFIERVIEW_H_
#define SRC_OPERATORS_QUANTIFIERVIEW_H_
#include <tuple>
#include <utility>
#include <vector>
#include "operators/operatorBase.h"

template <typename ExprType>
struct QuantifierView {
    struct Trigger : public IterAssignedTrigger<QuantifierView<ExprType>> {
        typedef QuantifierView<ExprType> View;
        virtual void exprUnrolled(const ExprType& expr) = 0;
        virtual void exprRolled(u_int64_t index, const ExprType& expr) = 0;
    };
    std::vector<ExprType> exprs;
    std::vector<std::shared_ptr<Trigger>> triggers;
    QuantifierView() {}
    QuantifierView(std::vector<ExprType>& exprs) : exprs(exprs) {}
    QuantifierView(std::vector<ExprType>&& exprs) : exprs(std::move(exprs)) {}

    virtual void initialUnroll() = 0;
    virtual void startTriggeringOnContainer() = 0;
    virtual void stopTriggeringOnContainer() = 0;
    virtual std::shared_ptr<QuantifierView<ExprType>>
    deepCopyQuantifierForUnroll(const AnyIterRef& iterator) = 0;
};

template <typename ExprType>
struct FixedArray : public QuantifierView<ExprType> {
    using QuantifierView<ExprType>::exprs;
    FixedArray(std::vector<ExprType>&& exprs)
        : QuantifierView<ExprType>(std::move(exprs)) {}
    FixedArray(std::vector<ExprType>& exprs)
        : QuantifierView<ExprType>(exprs) {}
    inline void initialUnroll() final {}
    inline void startTriggeringOnContainer() final {}
    inline void stopTriggeringOnContainer() final {}
    std::shared_ptr<QuantifierView<ExprType>> deepCopyQuantifierForUnroll(
        const AnyIterRef& iterator) final {
        auto newFixedArray =
            std::make_shared<FixedArray<ExprType>>(std::vector<ExprType>());
        newFixedArray->exprs.reserve(exprs.size());
        for (auto& expr : exprs) {
            newFixedArray->exprs.emplace_back(
                deepCopyForUnroll(expr, iterator));
        }
        return newFixedArray;
    }
};
#endif /* SRC_OPERATORS_QUANTIFIERVIEW_H_ */
