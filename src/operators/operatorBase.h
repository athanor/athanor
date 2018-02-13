#ifndef SRC_OPERATORS_OPERATORBASE_H_
#define SRC_OPERATORS_OPERATORBASE_H_
#include <type_traits>
#include <utility>

#include "operators/iterator.h"
#include "search/violationDescription.h"
#include "types/base.h"
#include "utils/cachedSharedPtr.h"

#define buildForOperators(f, sep)                                    \
    f(BoolValue) sep f(IntValue) sep f(SetValue) sep f(MSetValue)    \
        sep f(OpSetSize) sep f(OpSum) sep f(OpAnd) sep f(OpSetNotEq) \
            sep f(OpSetIntersect) sep sep f(OpIntEq) sep f(OpMod)    \
                sep f(OpProd) sep f(OpOr)

#define structDecls(name) struct name;
buildForOperators(structDecls, );
#undef structDecls

// bool returning
using BoolReturning =
    mpark::variant<ValRef<BoolValue>, IterRef<BoolValue>,
                   std::shared_ptr<OpAnd>, std::shared_ptr<OpSetNotEq>,
                   std::shared_ptr<OpIntEq>, std::shared_ptr<OpOr>>;

// int returning
using IntReturning =
    mpark::variant<ValRef<IntValue>, IterRef<IntValue>,
                   std::shared_ptr<OpSetSize>, std::shared_ptr<OpSum>,
                   std::shared_ptr<OpMod>, std::shared_ptr<OpProd>>;

// set returning
using SetReturning = mpark::variant<ValRef<SetValue>, IterRef<SetValue>,
                                    std::shared_ptr<OpSetIntersect>>;

// MSet returning
using MSetReturning = mpark::variant<ValRef<MSetValue>, IterRef<MSetValue>>;

// helper class for template magic, allows to test operators if they have a
// specific return type by testing if they are convertable to that type

#define returnTypeToValueAssociations(name)       \
    template <>                                   \
    struct AssociatedValueType<name##Returning> { \
        typedef name##Value type;                 \
    };
buildForAllTypes(returnTypeToValueAssociations, );
#undef returnTypeToValueAssociations
template <typename Op>

class ReturnType {
    // only one of the below test functions should ever be compiled, the rest
    // should be disabled.  the only function below that is compiled is the one
    // that can convert template type Op to the return  type mentioned in the
    // function (e.g. Setreturning, boolreturning)
#define testReturnTypeFunc(name)                                               \
    template <typename T>                                                      \
    static auto test()->decltype(std::declval<name##Returning&>() = std::move( \
                                     std::declval<std::shared_ptr<T>>()));     \
    template <typename T>                                                      \
    static auto test()->decltype(std::declval<name##Returning&>() =            \
                                     std::move(std::declval<T>()));

    buildForAllTypes(testReturnTypeFunc, );
#undef testReturnTypeFunc
   public:
    typedef BaseType<decltype(ReturnType<Op>::test<Op>())> type;
};

#
template <typename Operator, typename ReturnTypeIn>
using HasReturnType =
    std::is_same<ReturnTypeIn, typename ReturnType<Operator>::type>;

#define operatorFuncs(name)                                                    \
    template <>                                                                \
    std::shared_ptr<name> makeShared<name>();                                  \
    void reset(name& op);                                                      \
    void evaluate(name&);                                                      \
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

template <typename ExprType>
struct QuantifierView;

template <typename Operator>
inline void evaluate(Operator& op) {
    mpark::visit([&](auto& opImpl) { return evaluate(*opImpl); }, op);
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
