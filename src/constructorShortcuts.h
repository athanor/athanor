
#ifndef SRC_CONSTRUCTORSHORTCUTS_H_
#define SRC_CONSTRUCTORSHORTCUTS_H_
#include "operators/opIntEq.h"
#include "operators/opMod.h"
#include "operators/opSetForAll.h"
#include "operators/opSetNotEq.h"
#include "operators/opSetSize.h"
#include "types/allTypes.h"

template <typename... Operands>
std::shared_ptr<OpAnd> opAnd(Operands&&... operands) {
    return std::make_shared<OpAnd>(
        std::vector<BoolReturning>({std::forward<Operands>(operands)...}));
}

template <typename BoolReturningVec>
std::shared_ptr<OpAnd> opAnd(BoolReturningVec&& operands) {
    return std::make_shared<OpAnd>(std::forward<BoolReturningVec>(operands));
}
template <typename Set>
std::shared_ptr<OpSetSize> setSize(Set&& set) {
    return std::make_shared<OpSetSize>(std::forward<Set>(set));
}

template <typename Set>
std::shared_ptr<OpSetForAll> setForAll(Set&& set) {
    return std::make_shared<OpSetForAll>(std::forward<Set>(set));
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
#endif /* SRC_CONSTRUCTORSHORTCUTS_H_ */
