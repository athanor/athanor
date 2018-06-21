#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include <functional>
#include <iostream>
#include <memory>
#include <utility>
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"

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
class ViolationContainer;

typedef std::function<std::pair<bool, AnyExprRef>(AnyExprRef)>
    FindAndReplaceFunction;
struct AnyIterRef;
struct PathExtension;
struct ViolationContext;

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
    char flags = 0;
    typedef typename AssociatedTriggerType<View>::type TriggerType;

    inline bool isEvaluated() { return flags & 1; }
    inline void setEvaluated(bool set) {
        flags &= ~((char)1);
        flags |= set;
    }
    inline bool isConstant() { return flags & 2; }
    inline void setConstant(bool set) {
        flags &= ~((char)2);
        flags |= ((char)set) << 1;
    }

    virtual View& view();
    virtual const View& view() const;
    void addTrigger(const std::shared_ptr<TriggerType>& trigger,
                    bool includeMembers = true, Int memberIndex = -1) {
        if (!isConstant()) {
            addTriggerImpl(trigger, includeMembers, memberIndex);
        }
    }
    virtual void addTriggerImpl(const std::shared_ptr<TriggerType>& trigger,
                                bool includeMembers = true,
                                Int memberIndex = -1);
    inline void evaluate() {
        if (!isEvaluated()) {
            setEvaluated(true);
            this->evaluateImpl();
        }
    }
    virtual void evaluateImpl() = 0;
    void startTriggering() {
        if (!isConstant()) {
            this->startTriggeringImpl();
        }
    }
    virtual void startTriggeringImpl() = 0;
    virtual void stopTriggering() = 0;
    virtual void updateVarViolations(const ViolationContext& vioContext,
                                     ViolationContainer&) = 0;
    inline ExprRef<View> deepCopySelfForUnroll(const ExprRef<View>& self,
                                               const AnyIterRef& iterator) {
        if (isConstant()) {
            return self;
        }

        ExprRef<View> copy = deepCopySelfForUnrollImpl(self, iterator);
        copy->flags = flags;
        return copy;
    }
    virtual ExprRef<View> deepCopySelfForUnrollImpl(
        const ExprRef<View>& self, const AnyIterRef& iterator) const = 0;
    virtual void findAndReplaceSelf(const FindAndReplaceFunction&) = 0;

    virtual std::ostream& dumpState(std::ostream& os) const = 0;
    virtual std::pair<bool, ExprRef<View>> optimise(PathExtension path) = 0;
};

struct ViolationContext {
    UInt parentViolation;
    ViolationContext(UInt parentViolation) : parentViolation(parentViolation) {}
    virtual ~ViolationContext() {}
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
bool smallerValue(const AnyExprRef& u, const AnyExprRef& v);
bool largerValue(const AnyExprRef& u, const AnyExprRef& v);

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

template <typename Op, typename View>
inline bool isInstanceOf(const ExprRef<View>&) {
    return false;
}

struct PathExtension {
    PathExtension* parent;
    AnyExprRef expr;

   private:
    template <typename T>
    PathExtension(PathExtension* parent, T& expr)
        : parent(parent), expr(expr) {}

   public:
    template <typename T>
    PathExtension extend(T& expr) {
        return PathExtension(this, expr);
    }
    template <typename T>
    static PathExtension begin(T& expr) {
        return PathExtension(NULL, expr);
    }
};

extern UInt LARGE_VIOLATION;

#endif /* SRC_BASE_EXPRREF_H_ */
