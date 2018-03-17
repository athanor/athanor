
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

template <typename Val>
EnableIfValueAndReturn<Val, ValRef<Val>> deepCopy(const Val& src) {
    auto target = constructValueOfSameType(src);
    deepCopy(src, *target);
    return target;
}

template <typename InnerValueType>
inline std::ostream& prettyPrint(std::ostream& os,
                                 const ValRef<InnerValueType>& val) {
    return prettyPrint(os, *getViewPtr(val));
}

template <typename InnerValueType>
inline u_int64_t getValueHash(const ValRef<InnerValueType>& val) {
    return getValueHash(*getViewPtr(val));
}

template <typename InnerValueType>
inline std::ostream& operator<<(std::ostream& os,
                                const ValRef<InnerValueType>& val) {
    return prettyPrint(os, val);
}

template <typename ValueType>
ValRef<ValueType> constructValueOfSameType(const ValueType& other) {
    auto val = make<ValueType>();
    matchInnerType(other, *val);
    return val;
}

void normalise(AnyValRef& v);
bool smallerValue(const AnyValRef& u, const AnyValRef& v);
bool largerValue(const AnyValRef& u, const AnyValRef& v);
bool equalValue(const AnyValRef& u, const AnyValRef& v);
AnyValRef deepCopy(const AnyValRef& val);
std::ostream& operator<<(std::ostream& os, const AnyValRef& v);
u_int64_t getValueHash(const AnyValRef& v);
std::ostream& prettyPrint(std::ostream& os, const AnyValRef& v);

template <typename ViewType,
          typename ValueType = typename AssociatedValueType<ViewType>::type,
          typename std::enable_if<IsViewType<ViewType>::value, int>::type = 0>
inline ValueType& assumeAsValue(ViewType& view) {
    return reinterpret_cast<ValueType&>(view);
}

template <typename ViewType,
          typename ValueType = typename AssociatedValueType<ViewType>::type,
          typename std::enable_if<IsViewType<ViewType>::value, int>::type = 0>
inline const ValueType& assumeAsValue(const ViewType& view) {
    return reinterpret_cast<const ValueType&>(view);
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

#endif /* SRC_BASE_VALREF_H_ */
