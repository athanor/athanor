
#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include <iostream>
#include "base/exprRef.h"
#include "base/iterRef.h"

template <typename T>
struct ExprRef {
    typedef T element_type;
    union {
        ViewRef<T> viewRef;
        IterRef<T> iterRef;
        u_int64_t bits;
    };
    ExprRef(ViewRef<T> ref) : viewRef(std::move(ref)) { setViewRef(); }
    ExprRef(IterRef<T> ref) : iterRef(std::move(ref)) { setIterRef(); }
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
        setIterRef();
        return *this;
    }
    inline ExprRef<T>& operator=(const ViewRef<T>& ref) {
        viewRef = ref;
        setViewRef();
        return *this;
    }
    inline ExprRef<T> operator=(const ExprRef<T>& ref) {
        ref.visit([&](auto& ref) { this->operator=(ref); });
        return *this;
    }

   private:
    inline void setViewRef() { this->bits &= ~(((u_int64_t)1) << 63); }
    inline void setIterRef() { this->bits |= (((u_int64_t)1) << 63); }
    inline bool isViewRef() const {
        return (this->bits & (((u_int64_t)1) << 63)) == 0;
    }
    inline bool isIterRef() const {
        return (this->bits & (((u_int64_t)1) << 63)) != 0;
    }

   public:
    template <typename Func>
    inline decltype(auto) visit(Func&& func) const {
        if (isViewRef()) {
            return func(viewRef);
        } else
            return func(iterRef);
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
    inline auto& asViewRef() { return viewRef; }
    inline auto& asIterRef() { return iterRef; }
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
struct ExprType;

template <typename T>
struct ExprType<ExprRef<T>> {
    typedef T type;
};

template <typename T>
struct ExprType<std::vector<ExprRef<T>>> {
    typedef T type;
};

#define exprType(t) typename ExprType<BaseType<decltype(t)>>::type
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
    virtual ExprRef<View> deepCopyForUnroll(
        const ExprRef<View>& op, const AnyIterRef& iterator) const = 0;

    virtual std::ostream& dumpState(std::ostream& os) const = 0;
};

#endif /* SRC_BASE_EXPRREF_H_ */
