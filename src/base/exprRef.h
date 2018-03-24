
#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include <iostream>
#include <memory>
#include "base/iterRef.h"
#include "base/viewRef.h"
/*
template <typename T>
struct VisitorIterRefInvoker;
template <>
struct VisitorIterRefInvoker<void>;
template <typename T>
struct ExprRef {
    template <typename>
    friend struct VisitorIterRefInvoker;
    typedef T element_type;

   private:
    union {
        ViewRef<T> viewRef;
        IterRef<T> iterRef;
        u_int64_t bits;
    };

   public:
    ExprRef(ViewRef<T> ref) : viewRef(std::move(ref)) { clearIterRefMark(); }
    ExprRef(IterRef<T> ref) : iterRef(std::move(ref)) { setIterRefMark(); }
    ExprRef(const ExprRef<T>& other) { operator=(other); }
    ExprRef(ExprRef<T>&& other) {
        other.visit([&](auto& ref) { this->operator=(std::move(ref)); });
    }
    ~ExprRef() {
        visit([&](auto& ref) {
            typedef BaseType<decltype(ref)> Type;
            ref.~Type();
        });
    }
    inline ExprRef<T>& operator=(const IterRef<T>& ref) {
        iterRef = ref;
        setIterRefMark();
        return *this;
    }
    inline ExprRef<T>& operator=(const ViewRef<T>& ref) {
        viewRef = ref;
        clearIterRefMark();
        return *this;
    }
    inline ExprRef<T> operator=(const ExprRef<T>& ref) {
        ref.visit([&](auto& ref) { this->operator=(ref); });
        return *this;
    }

   private:
    inline void clearIterRefMark() { this->bits &= ~(((u_int64_t)1) << 63); }
    inline void setIterRefMark() { this->bits |= (((u_int64_t)1) << 63); }

   public:
    inline bool isViewRef() const {
        return (this->bits & (((u_int64_t)1) << 63)) == 0;
    }
    inline bool isIterRef() const {
        return (this->bits & (((u_int64_t)1) << 63)) != 0;
    }
    inline auto& asViewRef() {
        debug_code(assert(isViewRef()));
        return viewRef;
    }

    template <typename Func>
    inline decltype(auto) visit(Func&& func) const {
        if (isViewRef()) {
            return func(viewRef);
        } else {
            const_cast<ExprRef<T>*>(this)
                ->clearIterRefMark();  // pretend, simply  cleans set bits
            typedef decltype(func(iterRef)) ReturnType;
            return VisitorIterRefInvoker<ReturnType>::invokeAndClean(func,
                                                                     *this);
        }
    }

    template <typename Func>
    inline decltype(auto) visit(Func&& func) {
        if (isViewRef()) {
            return func(viewRef);
        } else {
            const_cast<ExprRef<T>*>(this)
                ->clearIterRefMark();  // pretend, simply  cleans set bits
            typedef decltype(func(iterRef)) ReturnType;
            return VisitorIterRefInvoker<ReturnType>::invokeAndClean(func,
                                                                     *this);
        }
    }

    inline T& operator*() {
        return visit([&](auto& ref) -> T& { return ref.operator*(); });
    }
    inline T* operator->() {
        return visit([&](auto& ref) { return ref.operator->(); });
    }
    inline T& operator*() const {
        return visit([&](auto& ref) -> T& { return ref.operator*(); });
    }
    inline T* operator->() const {
        return visit([&](auto& ref) { return ref.operator->(); });
    }
};

template <typename VisitorReturnType>
struct VisitorIterRefInvoker {
    template <typename Func, typename ExprRefType>
    inline static VisitorReturnType invokeAndClean(Func&& func,
                                                   ExprRefType& ref) {
        VisitorReturnType returnV = func(ref.iterRef);
        const_cast<BaseType<ExprRefType>&>(ref).setIterRefMark();
        return returnV;
    }
};
template <>
struct VisitorIterRefInvoker<void> {
    template <typename Func, typename ExprRefType>
    inline static void invokeAndClean(Func&& func, ExprRefType& ref) {
        func(ref.iterRef);
        const_cast<BaseType<ExprRefType>&>(ref).setIterRefMark();
    }
};
*/

template <typename T>
struct ExprRef {
    typedef T element_type;
    mpark::variant<ViewRef<T>, IterRef<T>> ref;
    ExprRef(ViewRef<T> ref) : ref(std::move(ref)) {}
    ExprRef(IterRef<T> ref) : ref(std::move(ref)) {}
    inline bool isViewRef() const {
        return mpark::get_if<ViewRef<T>>(&ref) != NULL;
    }
    inline bool isIterRef() const {
        return mpark::get_if<IterRef<T>>(&ref) != NULL;
    }
    inline auto& asViewRef() { return mpark::get<ViewRef<T>>(ref); }

    template <typename Func>
    inline decltype(auto) visit(Func&& func) const {
        return mpark::visit(std::forward<Func>(func), ref);
    }

    template <typename Func>
    inline decltype(auto) visit(Func&& func) {
        return mpark::visit(std::forward<Func>(func), ref);
    }

    inline T& operator*() {
        return visit([&](auto& ref) -> T& { return ref.operator*(); });
    }
    inline T* operator->() {
        return visit([&](auto& ref) { return ref.operator->(); });
    }
    inline T& operator*() const {
        return visit([&](auto& ref) -> T& { return ref.operator*(); });
    }
    inline T* operator->() const {
        return visit([&](auto& ref) { return ref.operator->(); });
    }
};
// variant for exprs
template <typename T>
using ExprRefMaker = ExprRef<typename AssociatedViewType<T>::type>;
typedef Variantised<ExprRefMaker> AnyExprRef;
// variant for vector of exprs
template <typename InnerExprType>
using ExprRefVec = std::vector<ExprRef<InnerExprType>>;
template <typename T>
using ExprRefVecMaker = ExprRefVec<typename AssociatedViewType<T>::type>;
typedef Variantised<ExprRefVecMaker> AnyExprVec;

template <typename T>
struct ViewType;
template <typename T>
struct ViewType<ExprRef<T>> {
    typedef T type;
};

template <typename T>
struct ViewType<std::vector<ExprRef<T>>> {
    typedef T type;
};

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const ExprRef<T>& ref) {
    return prettyPrint(os, *ref);
}
class ViolationDescription;
template <typename View>
struct ExprInterface {
    virtual void evaluate() = 0;
    virtual void startTriggering() = 0;
    virtual void stopTriggering() = 0;
    virtual void updateViolationDescription(u_int64_t parentViolation,
                                            ViolationDescription&) = 0;
    virtual ExprRef<View> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const = 0;

    virtual std::ostream& dumpState(std::ostream& os) const = 0;
    virtual inline std::pair<bool, std::pair<AnyValRef, AnyExprRef>>
    tryReplaceConstraintWithDefine() {
        return std::make_pair(false,
                              std::make_pair(ValRef<BoolValue>(nullptr),
                                             ViewRef<BoolView>(nullptr)));
    }
};
HashType getValueHash(const AnyExprRef& ref);
template <typename T>
inline ExprRef<T> deepCopyForUnrollOverload(const IterRef<T>& iterVal,
                                            const AnyIterRef& iterator) {
    const IterRef<T>* iteratorPtr = mpark::get_if<IterRef<T>>(&iterator);
    if (iteratorPtr != NULL &&
        iteratorPtr->getIterator().id == iterVal.getIterator().id) {
        return *iteratorPtr;
    } else {
        return iterVal;
    }
}
template <typename T>
ExprRef<T> deepCopyForUnrollOverload(const ViewRef<T>& view,
                                     const AnyIterRef& iterator) {
    if (dynamic_cast<typename AssociatedValueType<T>::type*>(&(*view))) {
        return view;
    } else {
        return view->deepCopySelfForUnroll(iterator);
    }
}
template <typename T>
ExprRef<T> deepCopyForUnroll(const ExprRef<T> expr,
                             const AnyIterRef& iterator) {
    return expr.visit(
        [&](auto& expr) { return deepCopyForUnrollOverload(expr, iterator); });
}
#endif /* SRC_BASE_EXPRREF_H_ */
