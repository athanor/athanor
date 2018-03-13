
#ifndef SRC_BASE_REFS_H_
#include <vector>
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"

#include "utils/variantOperations.h"

#include "common/common.h"
#define SRC_BASE_REFS_H_
template <typename T>
struct ValRef : public StandardSharedPtr<T> {
    using StandardSharedPtr<T>::StandardSharedPtr;
};

template <typename T>
struct ViewRef : public StandardSharedPtr<T> {
    using StandardSharedPtr<T>::StandardSharedPtr;
};

template <typename T>
std::shared_ptr<T> makeShared();

template <typename T,
          typename std::enable_if<IsValueType<T>::value, int>::type = 0>
ValRef<T> make() {
    return ValRef<T>(makeShared<T>());
}

// short cut for building a variant of any other templated class, where the
// class is templated over a value (SetValue,IntValue, etc.)
#define variantValues(V) T<V##Value>
template <template <typename> class T>
using Variantised =
    mpark::variant<buildForAllTypes(variantValues, MACRO_COMMA)>;
#undef variantValues

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

// variant for views
template <typename T>
using ViewRefMaker = ViewRef<typename AssociatedViewType<T>::type>;
typedef Variantised<ViewRefMaker> AnyViewRef;
// variant for vector of views
template <typename InnerViewType>
using ViewRefVec = std::vector<ViewRef<InnerViewType>>;
template <typename T>
using ViewRefVecMaker = ViewRefVec<typename AssociatedViewType<T>::type>;
typedef Variantised<ViewRefVecMaker> AnyViewVec;

template <typename T>
struct ViewType;

template <typename T>
struct ViewType<ViewRef<T>> {
    typedef T type;
};

template <typename T>
struct ViewType<std::vector<ViewRef<T>>> {
    typedef T type;
};

#define viewType(t) typename ViewType<BaseType<decltype(t)>>::type

// variant for domains
#define variantDomains(T) std::shared_ptr<T##Domain>
using AnyDomainRef =
    mpark::variant<buildForAllTypes(variantDomains, MACRO_COMMA)>;
#undef variantDomains

template <typename DomainType>
ValRef<typename AssociatedValueType<DomainType>::type> constructValueFromDomain(
    const DomainType& domain) {
    auto val = make<typename AssociatedValueType<DomainType>::type>();
    matchInnerType(domain, *val);
    return val;
}
template <typename ValueType>
ValRef<ValueType> constructValueOfSameType(const ValueType& other) {
    auto val = make<ValueType>();
    matchInnerType(other, *val);
    return val;
}

template <typename ViewType,
          typename ValueType = typename AssociatedValueType<ViewType>::type,
          typename std::enable_if<IsViewType<ViewType>::value, int>::type = 0>
inline ValRef<ValueType>& assumeAsValue(ViewRef<ViewType>& viewPtr) {
    return reinterpret_cast<ValRef<ValueType>&>(viewPtr);
}

template <typename ViewType,
          typename ValueType = typename AssociatedValueType<ViewType>::type,
          typename std::enable_if<IsViewType<ViewType>::value, int>::type = 0>
inline const ValRef<ValueType>& assumeAsValue(
    const ViewRef<ViewType>& viewPtr) {
    return reinterpret_cast<const ValRef<ValueType>&>(viewPtr);
}

template <typename ViewType,
          typename ValueType = typename AssociatedValueType<ViewType>::type,
          typename std::enable_if<IsViewType<ViewType>::value, int>::type = 0>
inline ValRef<ValueType>&& assumeAsValue(ViewRef<ViewType>&& viewPtr) {
    return reinterpret_cast<ValRef<ValueType>&&>(std::move(viewPtr));
}

template <typename ValueType,
          typename ViewType = typename AssociatedViewType<ValueType>::type,
          typename std::enable_if<IsValueType<ValueType>::value, int>::type = 0>
inline ViewRef<ViewType>& getViewPtr(ValRef<ValueType>& value) {
    return reinterpret_cast<ViewRef<ViewType>&>(value);
}

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

#endif /* SRC_BASE_REFS_H_ */
