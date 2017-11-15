
#ifndef SRC_OPERATORS_OPERATORBASE_H_
#define SRC_OPERATORS_OPERATORBASE_H_
#include "operators/quantifierBase.h"
#include "search/violationDescription.h"
#include "types/forwardDecls/typesAndDomains.h"
#include "utils/cachedSharedPtr.h"
#define buildForOperators(f, sep)                                           \
    f(IntValue) sep f(OpSetSize) sep f(OpSum) sep f(BoolValue) sep f(OpAnd) \
        sep f(OpSetNotEq) sep f(SetValue) sep f(OpSetIntersect)             \
            sep f(OpSetUnion)

#define structDecls(name) struct name;
buildForOperators(structDecls, );
#undef structDecls

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
                                            const IterValue& iterator);
buildForOperators(operatorFuncs, );
#undef operatorFuncs

// bool returning
using BoolReturning =
    mpark::variant<ValRef<BoolValue>, IterRef<BoolValue>,
                   std::shared_ptr<OpAnd>, std::shared_ptr<OpSetNotEq>>;

// int returning
using IntReturning =
    mpark::variant<ValRef<IntValue>, IterRef<IntValue>,
                   std::shared_ptr<OpSetSize>, std::shared_ptr<OpSum>>;

// set returning
using SetReturning = mpark::variant<ValRef<SetValue>, IterRef<SetValue>,
                                    std::shared_ptr<OpSetIntersect>>;

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

// overload for function deepCopyForUnrolling for all base operator variant
// types i.e. BoolReturning, IntReturning, SetReturning, etc.
template <typename ReturnType>
inline ReturnType deepCopyForUnroll(const ReturnType& expr,
                                    const IterValue& iterator) {
    return mpark::visit(
        [&](auto& exprImpl) -> ReturnType {
            return deepCopyForUnrollOverload(exprImpl, iterator);
        },
        expr);
}

/// overload of deepCopyForUnroll that catches operators
template <typename T>
std::shared_ptr<T> deepCopyForUnrollOverload(const std::shared_ptr<T>& ptr,
                                             const IterValue& iterator) {
    return deepCopyForUnroll(*ptr, iterator);
}

// ValRef overload for deepCopyOverload, note that ValRefs are not supposed to
// be deepCopied when unrolling
template <typename T>
inline ValRef<T> deepCopyForUnrollOverload(const ValRef<T>& val,
                                           const IterValue&) {
    return val;
}

template <typename T>
inline IterRef<T> deepCopyForUnrollOverload(const IterRef<T>& iterVal,
                                            const IterValue& iterator) {
    const IterRef<T>* iteratorPtr = mpark::get_if<IterRef<T>>(&iterator);
    if (iteratorPtr != NULL &&
        iteratorPtr->getIterator().id == iterVal.getIterator().id) {
        return *iteratorPtr;
    } else {
        return iterVal;
    }
}

#endif /* SRC_OPERATORS_OPERATORBASE_H_ */
