
#ifndef SRC_BASE_VALREF_H_
#define SRC_BASE_VALREF_H_
#include "base/exprRef.h"
#include "utils/variantOperations.h"

#include <vector>
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"

#include "common/common.h"
#include "utils/variantOperations.h"
template <typename Value>
struct ValRef : public StandardSharedPtr<Value> {
    using StandardSharedPtr<Value>::StandardSharedPtr;
    typedef typename AssociatedViewType<Value>::type View;
    ExprRef<View> asExpr();
    const ExprRef<View> asExpr() const;
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
template <typename Val>
EnableIfValueAndReturn<Val, ValRef<Val>> deepCopy(const Val& src) {
    auto target = constructValueOfSameType(src);
    deepCopy(src, *target);
    return target;
}

template <typename InnerValueType>
inline std::ostream& prettyPrint(std::ostream& os,
                                 const ValRef<InnerValueType>& val) {
    return prettyPrint(os, val.asExpr());
}

template <typename InnerValueType>
inline HashType getValueHash(const ValRef<InnerValueType>& val) {
    return getValueHash(val.asExpr());
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

AnyValRef deepCopy(const AnyValRef& val);
std::ostream& operator<<(std::ostream& os, const AnyValRef& v);
HashType getValueHash(const AnyValRef& v);
std::ostream& prettyPrint(std::ostream& os, const AnyValRef& v);

template <typename ViewType>
typename std::enable_if<
    IsViewType<ViewType>::value,
    const ValRef<typename AssociatedValueType<ViewType>::type>>::type
assumeAsValue(const ExprRef<ViewType>& viewPtr);

template <typename ViewType>
typename std::enable_if<
    IsViewType<ViewType>::value,
    ValRef<typename AssociatedValueType<ViewType>::type>>::type
assumeAsValue(ExprRef<ViewType>& viewPtr);

struct ValBase;
extern ValBase constantPool;

struct ValBase {
    UInt id = 0;
    ValBase* container = &constantPool;
    inline bool operator==(const ValBase& other) const {
        return id == other.id && container == other.container;
    }
};

template <typename Val>
EnableIfValueAndReturn<Val, ValBase&> valBase(Val& val);
template <typename Val>
EnableIfValueAndReturn<Val, const ValBase&> valBase(const Val& val);
const ValBase& valBase(const AnyValRef& ref);
ValBase& valBase(AnyValRef& ref);

#endif /* SRC_BASE_VALREF_H_ */
