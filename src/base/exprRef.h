#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"
#include "utils/OptionalRef.h"
#include "utils/flagSet.h"
extern bool verboseSanityError;
struct SanityCheckException {
    const std::string errorMessage;
    std::string file;
    int line = 0;
    std::vector<std::string> messageStack;
    std::string stateDump;
    SanityCheckException(const std::string& errorMessage)
        : errorMessage(errorMessage) {}
    SanityCheckException(const std::string& errorMessage,
                         const std::string& file, int line)
        : errorMessage(errorMessage), file(file), line(line) {}
    template <typename T>
    inline void report(T&& message) {
        messageStack.emplace_back(std::forward<T>(message));
    }
};

#define sanityCheck(check, message)                              \
    if (!(check)) {                                              \
        throw SanityCheckException(message, __FILE__, __LINE__); \
    }

#define sanityEqualsCheck(val1, val2)                              \
    sanityCheck(val1 == val2, toString(#val2, " should be ", val1, \
                                       " but is instead ", val2));
#define sanityLargeViolationCheck(val) \
    sanityCheck(                       \
        LARGE_VIOLATION == val,        \
        toString(#val, " should be LARGE_VIOLATION but is instead ", val));

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
struct ExprInterface;
template <typename View>
struct Undefinable {
    inline bool appearsDefined() const {
        const auto& child = static_cast<const ExprInterface<View>&>(*this);
        return child.flags
            .template get<typename ExprInterface<View>::AppearsDefinedFlag>();
    }
    inline void setAppearsDefined(bool set) {
        auto& child = static_cast<ExprInterface<View>&>(*this);
        child.flags
            .template get<typename ExprInterface<View>::AppearsDefinedFlag>() =
            set;
    }
};
template <>
struct Undefinable<BoolView> {
    inline bool appearsDefined() const { return true; }
};

template <typename View>
struct ExprInterface : public Undefinable<View> {
    typedef typename AssociatedTriggerType<View>::type TriggerType;

   private:
    friend struct Undefinable<View>;
    // flags for all ExprRefs
    struct EvaluatedFlag;
    struct IsConstantFlag;
    struct AppearsDefinedFlag;
    FlagSet<EvaluatedFlag, IsConstantFlag, AppearsDefinedFlag> flags;

   public:
    using Undefinable<View>::appearsDefined;
    inline bool isEvaluated() const {
        return flags.template get<EvaluatedFlag>();
    }
    inline void setEvaluated(bool set) {
        flags.template get<EvaluatedFlag>() = set;
    }
    inline bool isConstant() const {
        return flags.template get<IsConstantFlag>();
    }
    inline void setConstant(bool set) {
        flags.template get<IsConstantFlag>() = set;
    }

    virtual ~ExprInterface() {}
    virtual OptionalRef<View> view();
    virtual OptionalRef<const View> view() const;

    inline OptionalRef<View> getViewIfDefined() {
        if (appearsDefined()) {
            return view();
        } else {
            return EmptyOptional();
        }
    }
    inline OptionalRef<const View> getViewIfDefined() const {
        if (appearsDefined()) {
            return view();
        } else {
            return EmptyOptional();
        }
    }

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
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer& vioContainer) {
        if (!isConstant()) {
            updateVarViolationsImpl(vioContext, vioContainer);
        }
    }
    virtual void updateVarViolationsImpl(const ViolationContext& vioContext,
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
    inline void debugSanityCheck() {
        try {
            debugSanityCheckImpl();
        } catch (SanityCheckException& e) {
            if (verboseSanityError && e.stateDump.empty()) {
                std::ostringstream os;
                this->dumpState(os);
                e.stateDump = os.str();
            }
            e.report(toString("From operator ", this->getOpName(),
                              " with value ", this->getViewIfDefined()));
            throw e;
        }
    }
    virtual void debugSanityCheckImpl() const = 0;
    virtual std::string getOpName() const = 0;
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

inline bool appearsDefined(const AnyExprRef& expr) {
    return mpark::visit([&](auto& expr) { return expr->appearsDefined(); },
                        expr);
}
std::ostream& prettyPrint(std::ostream& os, const AnyExprRef& expr);

template <typename View>
inline std::ostream& prettyPrint(std::ostream& os,
                                 const OptionalRef<View>& ref) {
    if (ref) {
        return prettyPrint(os, *ref);
    } else {
        return os << "undefined";
    }
}

bool smallerValue(const AnyExprRef& u, const AnyExprRef& v);

template <typename View>
bool smallerValue(const OptionalRef<View>& left,
                  const OptionalRef<View>& right) {
    return left && right && smallerValue(*left, *right);
}
bool largerValue(const AnyExprRef& u, const AnyExprRef& v);

template <typename View>
bool largerValue(const OptionalRef<View>& left,
                 const OptionalRef<View>& right) {
    return left && right && largerValue(*left, *right);
}

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
extern BoolView& VIOLATING_BOOL_VIEW;
extern UInt MAX_DOMAIN_SIZE;
#endif /* SRC_BASE_EXPRREF_H_ */
