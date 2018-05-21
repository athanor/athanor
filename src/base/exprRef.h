
#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"
struct AnyIterRef;
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
typedef std::function<std::pair<bool, AnyExprRef>(AnyExprRef)>
    FindAndReplaceFunction;

template <typename View>
struct Undefinable {
    virtual bool isUndefined() = 0;
};
template <>
struct Undefinable<BoolView> {
    inline bool isUndefined() { return false; }
};
template <typename View>
struct ExprInterface : public Undefinable<View> {
    bool evaluated = false;
    typedef typename AssociatedTriggerType<View>::type TriggerType;
    virtual View& view();
    virtual const View& view() const;
    virtual void addTrigger(const std::shared_ptr<TriggerType>& trigger,
                            bool includeMembers = true);
    inline void evaluate() {
        if (!evaluated) {
            evaluated = true;
            this->evaluateImpl();
        }
    }
    virtual void evaluateImpl() = 0;
    virtual void startTriggering() = 0;
    virtual void stopTriggering() = 0;
    virtual void updateViolationDescription(UInt parentViolation,
                                            ViolationDescription&) = 0;
    virtual ExprRef<View> deepCopySelfForUnroll(
        const ExprRef<View>& self, const AnyIterRef& iterator) const = 0;
    virtual void findAndReplaceSelf(const FindAndReplaceFunction&) = 0;

    virtual std::ostream& dumpState(std::ostream& os) const = 0;
    virtual bool optimise() = 0;
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

#define viewType(t) typename ::ViewType<BaseType<decltype(t)>>::type

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const ExprRef<T>& ref) {
    return prettyPrint(os, ref->view());
}
HashType getValueHash(const AnyExprRef& ref);
std::ostream& prettyPrint(std::ostream& os, const AnyExprRef& expr);

template <typename T>
inline ExprRef<T> findAndReplace(ExprRef<T>& expr,
                                 const FindAndReplaceFunction& func) {
    auto newExpr = func(expr);
    if (newExpr.first) {
        return mpark::get<ExprRef<T>>(newExpr.second);
    } else {
        expr->findAndReplaceSelf(func);
        return expr;
    }
}

extern UInt LARGE_VIOLATION;

#endif /* SRC_BASE_EXPRREF_H_ */
