
#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include "base/iterRef.h"
#include "base/viewRef.h"
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
    ~ExprRef() {
        visit([&](auto& ref) { ref = BaseType<decltype(ref)>(nullptr); });
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
        ref->visit([&](auto& ref) { this->operator=(ref); });
        return *this;
    }

   private:
    inline void setViewRef() { this->bits &= ~(((u_int64_t)1) << 63); }
    inline void setIterRef() { this->bits |= (((u_int64_t)1) << 63); }
    inline bool isViewRef() {
        return (this->bits & (((u_int64_t)1) << 63)) == 0;
    }
    inline bool isIterRef() {
        return (this->bits & (((u_int64_t)1) << 63)) != 0;
    }

   public:
    template <typename Func>
    inline decltype(auto) visit(Func&& func) const {
        if (isViewRef()) {
            return func(this->viewRef);
        } else
            return func(this->iterRef);
    }

    inline decltype(auto) operator*() {
        return visit([&](auto& ref) { return ref.operator*(); });
    }
    inline decltype(auto) operator-> () {
        return visit([&](auto& ref) { return ref.operator->(); });
    }
    inline decltype(auto) operator*() const {
        return visit([&](auto& ref) { return ref.operator*(); });
    }
    inline decltype(auto) operator-> () const {
        return visit([&](auto& ref) { return ref.operator->(); });
    }
};
#endif /* SRC_BASE_EXPRREF_H_ */
