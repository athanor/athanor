#ifndef SRC_BASE_VIEWREF_H_
#define SRC_BASE_VIEWREF_H_
#include <memory>
#include <vector>
#include "base/intSize.h"
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"
#include "base/valRef.h"
#include "common/common.h"
#include "utils/variantOperations.h"

template <typename T>
struct ViewRef : public StandardSharedPtr<T> {
    using StandardSharedPtr<T>::StandardSharedPtr;
};

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

#define viewType(t) typename ::ViewType<BaseType<decltype(t)>>::type

template <typename T = int>
inline HashType getValueHash(const AnyViewRef& view, T = 0) {
    return mpark::visit(
        [&](const auto& viewImpl) { return getValueHash(*viewImpl); }, view);
}

template <typename View>
inline EnableIfViewAndReturn<View, std::ostream&> operator<<(
    std::ostream& os, const ViewRef<View>& v) {
    return prettyPrint(os, *v);
}

template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyViewRef& v, T = 0) {
    return mpark::visit(
        [&os](auto& vImpl) -> std::ostream& { return prettyPrint(os, vImpl); },
        v);
}

#endif /* SRC_BASE_VIEWREF_H_ */
