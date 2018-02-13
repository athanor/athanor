
#ifndef SRC_CONSTRUCTORSHORTCUTS_H_
#define SRC_CONSTRUCTORSHORTCUTS_H_
#include "operators/opIntEq.h"
#include "operators/opMod.h"
#include "operators/opOr.h"
#include "operators/opProd.h"
#include "operators/opSetNotEq.h"
#include "operators/opSetSize.h"
#include "operators/opSubset.h"
#include "operators/opSum.h"
#include "operators/quantifier.h"
#include "types/allTypes.h"

template <typename Operand, typename... Operands>
typename std::enable_if<
    !std ::is_base_of<QuantifierView<BoolReturning>,
                      typename BaseType<Operand>::element_type>::value,
    std::shared_ptr<OpAnd>>::type
opAnd(Operand&& operand, Operands&&... operands) {
    return std::make_shared<OpAnd>(std::make_shared<FixedArray<BoolReturning>>(
        std::vector<BoolReturning>({std::forward<Operand>(operand),
                                    std::forward<Operands>(operands)...})));
}

template <typename Quant,
          typename std::enable_if<
              std ::is_base_of<QuantifierView<BoolReturning>,
                               typename BaseType<Quant>::element_type>::value,
              int>::type = 0>
inline auto opAnd(Quant&& quant) {
    return std::make_shared<OpAnd>(std::forward<Quant>(quant));
}

template <typename Operand, typename... Operands>
typename std::enable_if<
    !std ::is_base_of<QuantifierView<BoolReturning>,
                      typename BaseType<Operand>::element_type>::value,
    std::shared_ptr<OpOr>>::type
opOr(Operand&& operand, Operands&&... operands) {
    return std::make_shared<OpOr>(std::make_shared<FixedArray<BoolReturning>>(
        std::vector<BoolReturning>({std::forward<Operand>(operand),
                                    std::forward<Operands>(operands)...})));
}

template <typename Quant,
          typename std::enable_if<
              std ::is_base_of<QuantifierView<BoolReturning>,
                               typename BaseType<Quant>::element_type>::value,
              int>::type = 0>
inline auto opOr(Quant&& quant) {
    return std::make_shared<OpOr>(std::forward<Quant>(quant));
}

template <typename Operand, typename... Operands>
typename std::enable_if<
    !std ::is_base_of<QuantifierView<IntReturning>,
                      typename BaseType<Operand>::element_type>::value,
    std::shared_ptr<OpSum>>::type
sum(Operand&& operand, Operands&&... operands) {
    return std::make_shared<OpSum>(std::make_shared<FixedArray<IntReturning>>(
        std::vector<IntReturning>({std::forward<Operand>(operand),
                                   std::forward<Operands>(operands)...})));
}

template <typename Quant,
          typename std::enable_if<
              std ::is_base_of<QuantifierView<IntReturning>,
                               typename BaseType<Quant>::element_type>::value,
              int>::type = 0>
inline auto sum(Quant&& quant) {
    return std::make_shared<OpSum>(std::forward<Quant>(quant));
}

template <typename... Operands>
std::shared_ptr<OpProd> opProd(Operands&&... operands) {
    return std::make_shared<OpProd>(
        std::vector<IntReturning>({std::forward<Operands>(operands)...}));
}

template <typename IntReturningVec>
std::shared_ptr<OpProd> opProd(IntReturningVec&& operands) {
    return std::make_shared<OpProd>(std::forward<IntReturningVec>(operands));
}

template <typename Set>
std::shared_ptr<OpSetSize> setSize(Set&& set) {
    return std::make_shared<OpSetSize>(std::forward<Set>(set));
}

template <typename ExprType, typename ContainerType>
auto quant(ContainerType&& container) {
    return std::make_shared<Quantifier<
        typename ReturnType<BaseType<ContainerType>>::type, ExprType>>(
        std::forward<ContainerType>(container));
}
template <typename L, typename R>
std::shared_ptr<OpIntEq> intEq(L&& l, R&& r) {
    return std::make_shared<OpIntEq>(std::forward<L>(l), std::forward<R>(r));
}
template <typename L, typename R>
std::shared_ptr<OpMod> mod(L&& l, R&& r) {
    return std::make_shared<OpMod>(std::forward<L>(l), std::forward<R>(r));
}
template <typename L, typename R>
std::shared_ptr<OpSetNotEq> setNotEq(L&& l, R&& r) {
    return std::make_shared<OpSetNotEq>(std::forward<L>(l), std::forward<R>(r));
}
template <typename L, typename R>
std::shared_ptr<OpSubset> subset(L&& l, R&& r) {
    return std::make_shared<OpSubset>(std::forward<L>(l), std::forward<R>(r));
}
#endif /* SRC_CONSTRUCTORSHORTCUTS_H_ */
