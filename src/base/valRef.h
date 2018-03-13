
#ifndef SRC_BASE_VALREF_H_
#define SRC_BASE_VALREF_H_
#include "utils/variantOperations.h"

#include <vector>
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"
#include "base/viewRef.h"
#include "common/common.h"
#include "utils/variantOperations.h"
template <typename T>
struct ValRef : public StandardSharedPtr<T> {
    using StandardSharedPtr<T>::StandardSharedPtr;
};

template <typename T>
std::shared_ptr<T> makeShared();

template <typename T,
          typename std::enable_if<IsValueType<T>::value, int>::type = 0>
ValRef<T> make() {
    return ValRef<T>(makeShared<T>());
}

// variant for values
typedef Variantised<ValRef> AnyValRef;
// variant for vector of values
template <typename InnerValueType>
using ValRefVec = std::vector<ValRef<InnerValueType>>;
typedef Variantised<ValRefVec> AnyValVec;

template <typename T>
struct ValType;

template <typename T>
struct ValType<ValRef<T>> {
    typedef T type;
};

template <typename T>
struct ValType<std::vector<ValRef<T>>> {
    typedef T type;
};

#define valType(t) typename ValType<BaseType<decltype(t)>>::type

template <typename T>
struct ViewRef;

template <typename ValueType,
          typename ViewType = typename AssociatedViewType<ValueType>::type,
          typename std::enable_if<IsValueType<ValueType>::value, int>::type = 0>
inline const ViewRef<ViewType>& getViewPtr(const ValRef<ValueType>& value) {
    return reinterpret_cast<const ViewRef<ViewType>&>(value);
}

template <typename ValueType,
          typename ViewType = typename AssociatedViewType<ValueType>::type,
          typename std::enable_if<IsValueType<ValueType>::value, int>::type = 0>
inline ViewRef<ViewType>&& getViewPtr(ValRef<ValueType>&& value) {
    return reinterpret_cast<ViewRef<ViewType>&&>(std::move(value));
}

template <typename T = int>
inline bool smallerValue(const AnyValRef& u, const AnyValRef& v, T = 0) {
    return u.index() < v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return smallerValue(*uImpl, *vImpl);
                },
                u));
}

template <typename T = int>
inline void normalise(AnyValRef& v, T = 0) {
    mpark::visit([](auto& vImpl) { normalise(*vImpl); }, v);
}

template <typename T = int>
inline bool largerValue(const AnyValRef& u, const AnyValRef& v, T = 0) {
    return u.index() > v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return largerValue(*uImpl, *vImpl);
                },
                u));
}

template <typename T = int>
inline bool equalValue(const AnyValRef& u, const AnyValRef& v, T = 0) {
    return (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return equalValue(*uImpl, *vImpl);
                },
                u));
}

template <typename Val>
EnableIfValueAndReturn<Val, ValRef<Val>> deepCopy(const Val& src) {
    auto target = constructValueOfSameType(src);
    deepCopy(src, *target);
    return target;
}

template <typename T = int>
inline AnyValRef deepCopy(const AnyValRef& val, T = 0) {
    return mpark::visit(
        [](const auto& valImpl) { return AnyValRef(deepCopy(*valImpl)); }, val);
}

template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyValRef& v, T = 0) {
    return mpark::visit(
        [&os](auto& vImpl) -> std::ostream& {
            return prettyPrint(os, *getViewPtr(vImpl));
        },
        v);
}
template <typename ValueType>
ValRef<ValueType> constructValueOfSameType(const ValueType& other) {
    auto val = make<ValueType>();
    matchInnerType(other, *val);
    return val;
}

#endif /* SRC_BASE_VALREF_H_ */
