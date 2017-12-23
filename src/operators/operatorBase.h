#ifndef SRC_OPERATORS_OPERATORBASE_H_
#define SRC_OPERATORS_OPERATORBASE_H_
#include <type_traits>
#include <utility>
#include "operators/quantifierBase.h"
#include "search/violationDescription.h"
#include "types/base.h"
#include "utils/cachedSharedPtr.h"
#define buildForOperators(f, sep)                                           \
    f(IntValue) sep f(OpSetSize) sep f(OpSum) sep f(BoolValue) sep f(OpAnd) \
        sep f(OpSetNotEq) sep f(SetValue) sep f(OpSetIntersect)             \
            sep f(OpSetForAll) sep f(OpIntEq) sep f(OpMod) sep f(OpProd)

#define structDecls(name) struct name;
buildForOperators(structDecls, );
#undef structDecls

// bool returning
using BoolReturning =
    mpark::variant<ValRef<BoolValue>, IterRef<BoolValue>,
                   std::shared_ptr<OpAnd>, std::shared_ptr<OpSetNotEq>,
                   std::shared_ptr<OpSetForAll>, std::shared_ptr<OpIntEq>>;

// int returning
using IntReturning =
    mpark::variant<ValRef<IntValue>, IterRef<IntValue>,
                   std::shared_ptr<OpSetSize>, std::shared_ptr<OpSum>,
                   std::shared_ptr<OpMod>, std::shared_ptr<OpProd>>;

// set returning
using SetReturning = mpark::variant<ValRef<SetValue>, IterRef<SetValue>,
                                    std::shared_ptr<OpSetIntersect>>;
template <typename Op>
class ReturnType {
    template <typename T>
    static auto test() -> decltype(std::declval<SetReturning&>() = std::move(
                                       std::declval<std::shared_ptr<T>>()));
    template <typename T>
    static auto test() -> decltype(std::declval<BoolReturning&>() = std::move(
                                       std::declval<std::shared_ptr<T>>()));
    template <typename T>
    static auto test() -> decltype(std::declval<IntReturning&>() = std::move(
                                       std::declval<std::shared_ptr<T>>()));

   public:
    typedef BaseType<decltype(ReturnType<Op>::test<Op>())> type;
};
template <typename Operator, typename ReturnTypeIn>
using HasReturnType =
    std::is_same<ReturnTypeIn, typename ReturnType<Operator>::type>;
// hack just to specialise the return type of some of the
// evaluate functions

template <typename T>
using SetMembersVectorImpl = std::vector<ValRef<T>>;

using SetMembersVector = Variantised<SetMembersVectorImpl>;

template <typename T>
struct EvaluateResult {
    typedef void type;
};

template <>
struct EvaluateResult<SetReturning> {
    typedef SetMembersVector type;
};

#define operatorFuncs(name)                                                    \
    template <>                                                                \
    std::shared_ptr<name> makeShared<name>();                                  \
    void reset(name& op);                                                      \
    typename EvaluateResult<typename ReturnType<name>::type>::type evaluate(   \
        name&);                                                                \
    void startTriggering(name&);                                               \
    void stopTriggering(name&);                                                \
    void updateViolationDescription(const name& op, u_int64_t parentViolation, \
                                    ViolationDescription&);                    \
    std::shared_ptr<name> deepCopyForUnroll(const name& op,                    \
                                            const AnyIterRef& iterator);       \
                                                                               \
    std::ostream& dumpState(std::ostream& os, const name& val);
buildForOperators(operatorFuncs, );
#undef operatorFuncs

template <typename View, typename Operator>
inline View& getView(const Operator& op) {
    return mpark::visit(
        [&](auto& opImpl) -> View& { return reinterpret_cast<View&>(*opImpl); },
        op);
}

template <typename Operator>
inline decltype(auto) evaluate(Operator& op) {
    return mpark::visit([&](auto& opImpl) { return evaluate(*opImpl); }, op);
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

// overload for function deepCopyForUnrolling for all base operator variant
// types i.e. BoolReturning, IntReturning, SetReturning, etc.
template <typename ReturnType>
inline ReturnType deepCopyForUnroll(const ReturnType& expr,
                                    const AnyIterRef& iterator) {
    return mpark::visit(
        [&](auto& exprImpl) -> ReturnType {
            return deepCopyForUnrollOverload(exprImpl, iterator);
        },
        expr);
}

/// overload of deepCopyForUnroll that catches operators
template <typename T>
std::shared_ptr<T> deepCopyForUnrollOverload(const std::shared_ptr<T>& ptr,
                                             const AnyIterRef& iterator) {
    return deepCopyForUnroll(*ptr, iterator);
}

// ValRef overload for deepCopyOverload, note that ValRefs are not supposed to
// be deepCopied when unrolling
template <typename T>
inline ValRef<T> deepCopyForUnrollOverload(const ValRef<T>& val,
                                           const AnyIterRef&) {
    return val;
}

template <typename T>
inline IterRef<T> deepCopyForUnrollOverload(const IterRef<T>& iterVal,
                                            const AnyIterRef& iterator) {
    const IterRef<T>* iteratorPtr = mpark::get_if<IterRef<T>>(&iterator);
    if (iteratorPtr != NULL &&
        iteratorPtr->getIterator().id == iterVal.getIterator().id) {
        return *iteratorPtr;
    } else {
        return iterVal;
    }
}

template <typename Operator>
inline std::ostream& dumpState(std::ostream& os, const Operator& op) {
    return mpark::visit(
        [&](auto& opImpl) -> std::ostream& { return dumpState(os, *opImpl); },
        op);
}

#endif /* SRC_OPERATORS_OPERATORBASE_H_ */
