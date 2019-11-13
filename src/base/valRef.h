
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

template <typename Val>
EnableIfValueAndReturn<Val, ValRef<Val>> make();
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
extern ValBase variablePool;
extern ValBase inlinedPool;
// checks if v is one of the global pools like inlinedPool or variablePool for
// example.
bool isPoolMarker(const ValBase* v);
struct ValBase {
    UInt id = 0;
    ValBase* container = &constantPool;
    virtual ~ValBase() {}
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

template <typename Val>
EnableIfValueAndReturn<Val, bool> hasVariableSize(const Val& val);
template <typename Val>
EnableIfValueAndReturn<Val, UInt> getSize(const Val& val);

template <typename Val, typename MemberExprType>
inline void varBaseSanityChecks(
    const Val& val, const ExprRefVec<MemberExprType>& valMembersImpl) {
    for (size_t i = 0; i < valMembersImpl.size(); i++) {
        const ValBase& base = valBase(*assumeAsValue(valMembersImpl[i]));
        sanityCheck(
            base.container == &val,
            toString("Member with id ", base.id, " does not point to parent."));
        sanityEqualsCheck(base.id, i);
    }
}
#endif /* SRC_BASE_VALREF_H_ */
