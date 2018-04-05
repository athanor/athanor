
#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include <functional>
#include <iostream>
#include <memory>
#include <utility>


template <typename View>
struct ExprRef;

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

class ViolationDescription;
typedef std::function<AnyExprRef(AnyExprRef)> FindAndReplaceFunction;

template <typename View>
struct ExprInterface {
    virtual inline View& view() { return *static_cast<View*>(this); }
    virtual inline const View& view() const {
        return *static_cast<View*>(this);
    }
    virtual void evaluate() = 0;
    virtual void startTriggering() = 0;
    virtual void stopTriggering() = 0;
    virtual void updateViolationDescription(UInt parentViolation,
                                            ViolationDescription&) = 0;
    virtual ExprRef<View> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const = 0;
    virtual void findAndReplaceSelf(const FindAndReplaceFunction&) = 0;

    virtual std::ostream& dumpState(std::ostream& os) const = 0;
};

template <typename View>
struct ExprRef : public StandardSharedPtr<ExprInterface<View>> {
    using StandardSharedPtr<ExprInterface<View>>::StandardSharedPtr;
};

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
    return prettyPrint(os, ref->view());
}
HashType getValueHash(const AnyExprRef& ref);
std::ostream& prettyPrint(std::ostream& os, const AnyExprRef& expr);

#endif /* SRC_BASE_EXPRREF_H_ */
