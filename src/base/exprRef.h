#ifndef SRC_BASE_EXPRREF_H_
#define SRC_BASE_EXPRREF_H_
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <utility>
#include "base/standardSharedPtr.h"
#include "base/typeDecls.h"
#include <optional>
#include "utils/OptionalRef.h"
#include "utils/filename.h"
#include "utils/flagSet.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
extern bool sanityCheckRepeatMode;
extern bool hashCheckRepeatMode;
extern bool repeatSanityCheckOfConst;
extern bool dontSkipSanityCheckForAlreadyVisitedChildren;
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

#define sanityCheck(check, message)                                  \
    if (!(check)) {                                                  \
        throw SanityCheckException(message, __FILENAME__, __LINE__); \
    }

#define sanityEqualsCheck(val1, val2)                              \
    sanityCheck(val1 == val2, toString(#val2, " should be ", val1, \
                                       " but is instead ", val2));
#define sanityLargeViolationCheck(val) \
    sanityCheck(                       \
        LARGE_VIOLATION == val,        \
        toString(#val, " should be LARGE_VIOLATION but is instead ", val));

#define hashCollision(message)                                                \
    throw SanityCheckException(toString("Hash Collision assumed: ", message), \
                               __FILENAME__, __LINE__);

template <typename View>
struct ExprInterface;

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
struct PathExtension;
typedef std::function<std::pair<bool, AnyExprRef>(AnyExprRef,
                                                  const PathExtension&)>
    FindAndReplaceFunction;

struct AnyIterRef;
template <typename Trigger, bool print = false>
class TriggerOwner;

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

struct PathExtension {
    PathExtension* parent;
    AnyExprRef expr;
    bool isCondition;

   private:
    PathExtension(PathExtension* parent, AnyExprRef expr, bool isCondition)
        : parent(parent), expr(std::move(expr)), isCondition(isCondition) {}

   public:
    inline bool isTop() const {
        return lib::visit([&](auto& expr) -> bool { return !expr; }, expr);
    }
    PathExtension extend(AnyExprRef expr, bool markAsCondition = false) {
        return PathExtension(this, std::move(expr),
                             isCondition || markAsCondition);
    }
    static inline PathExtension begin(bool markAsCondition = false) {
        return PathExtension(NULL, ExprRef<BoolView>(nullptr), markAsCondition);
    }
};

struct ViolationContext;
template <typename View>
bool optimise(AnyExprRef parentExpr, ExprRef<View>& self, PathExtension& path,
              bool markAsCondition = false);
template <typename View>
bool optimise(ExprRef<View>& expr, bool markAsCondition = false);

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
    struct SanityCheckedOnceFlag;
    struct SanityCheckRepeatFlag;
    struct DetectNoHashesFlag;
    struct HashCheckedOnceFlag;
    struct HashCheckRepeatFlag;

    FlagSet<EvaluatedFlag, IsConstantFlag, AppearsDefinedFlag,
            SanityCheckedOnceFlag, SanityCheckRepeatFlag, DetectNoHashesFlag,
            HashCheckedOnceFlag, HashCheckRepeatFlag>
        flags;

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

   protected:
    virtual void addTriggerImpl(const std::shared_ptr<TriggerType>& trigger,
                                bool includeMembers = true,
                                Int memberIndex = -1);

    virtual void evaluateImpl() = 0;

    virtual void startTriggeringImpl() = 0;

    virtual void updateVarViolationsImpl(const ViolationContext& vioContext,
                                         ViolationContainer&) = 0;

    virtual ExprRef<View> deepCopyForUnrollImpl(
        const ExprRef<View>& self, const AnyIterRef& iterator) const = 0;

    virtual void debugSanityCheckImpl() const = 0;

   public:
    void addTrigger(const std::shared_ptr<TriggerType>& trigger,
                    bool includeMembers = true, Int memberIndex = -1) {
        if (!isConstant()) {
            addTriggerImpl(trigger, includeMembers, memberIndex);
        }
    }

    template <typename TriggerSubType>
    void addTrigger(const TriggerOwner<TriggerSubType>& trigger,
                    bool includeMembers = true, Int memberIndex = -1) {
        this->addTrigger(trigger.getTrigger(), includeMembers, memberIndex);
    }
    inline void evaluate() {
        if (!isEvaluated()) {
            setEvaluated(true);
            this->evaluateImpl();
        }
    }
    void startTriggering() {
        if (!isConstant()) {
            this->startTriggeringImpl();
        }
    }

    virtual void stopTriggering() = 0;
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer& vioContainer) {
        if (!isConstant()) {
            updateVarViolationsImpl(vioContext, vioContainer);
        }
    }

    inline ExprRef<View> deepCopyForUnroll(const ExprRef<View>& self,
                                           const AnyIterRef& iterator) {
        if (isConstant()) {
            return self;
        }

        ExprRef<View> copy = deepCopyForUnrollImpl(self, iterator);
        copy->flags = flags;
        return copy;
    }
    virtual void findAndReplaceSelf(const FindAndReplaceFunction&,
                                    PathExtension path) = 0;

    virtual std::ostream& dumpState(std::ostream& os) const = 0;

    virtual std::pair<bool, ExprRef<View>> optimiseImpl(ExprRef<View>& self,
                                                        PathExtension path) = 0;

    inline void debugSanityCheck() const {
        auto sanityCheckedOnce = flags.template get<SanityCheckedOnceFlag>();
        auto sanityCheckRepeat = flags.template get<SanityCheckRepeatFlag>();
        // maybe don't repeat sanity checks of const expr, depending on flags
        if (!repeatSanityCheckOfConst && sanityCheckedOnce && isConstant()) {
            return;
        }
        // maybe don't sanity check exprs already visited in this iteration
        if (dontSkipSanityCheckForAlreadyVisitedChildren && sanityCheckedOnce &&
            sanityCheckRepeat == sanityCheckRepeatMode) {
            return;
        }
        {
            auto& constThis = const_cast<ExprInterface<View>&>(*this);
            constThis.flags.template get<SanityCheckedOnceFlag>() = true;
            constThis.flags.template get<SanityCheckRepeatFlag>() =
                sanityCheckRepeatMode;
        }
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
    virtual std::string getOpName() const = 0;
    virtual inline bool isQuantifier() const { return false; }
    virtual inline void hashChecksImpl() const { shouldNotBeCalledPanic; }
    inline void hashChecks() const {
        auto hashCheckedOnce = flags.template get<HashCheckedOnceFlag>();
        auto hashCheckRepeat = flags.template get<HashCheckRepeatFlag>();
        auto detectNoHashes = flags.template get<DetectNoHashesFlag>();

        if (detectNoHashes) {
            return;
        }
        // don't repeat hash checks of const
        if (hashCheckedOnce && isConstant()) {
            return;
        }
        // don't hash check exprs already visited in this iteration
        if (hashCheckedOnce && hashCheckRepeat == hashCheckRepeatMode) {
            return;
        }
        {
            auto& constThis = const_cast<ExprInterface<View>&>(*this);
            constThis.flags.template get<HashCheckedOnceFlag>() = true;
            constThis.flags.template get<HashCheckRepeatFlag>() =
                hashCheckRepeatMode;
        }
        try {
            hashChecksImpl();
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
};

template <typename View>
bool optimise(AnyExprRef parentExpr, ExprRef<View>& self, PathExtension& path,
              bool markAsCondition) {
    if (self->isConstant()) {
        return false;
    }
    auto boolExprPair =
        self->optimiseImpl(self, path.extend(parentExpr, markAsCondition));
    if (boolExprPair.first) {
        self = boolExprPair.second;
    }
    return boolExprPair.first;
}

template <typename View>
bool optimise(ExprRef<View>& expr, bool markAsCondition) {
    if (expr->isConstant()) {
        return false;
    }
    auto boolExprPair =
        expr->optimiseImpl(expr, PathExtension::begin(markAsCondition));
    if (boolExprPair.first) {
        expr = boolExprPair.second;
    }
    return boolExprPair.first;
}

struct ViolationContext {
    UInt parentViolation;
    ViolationContext(UInt parentViolation) : parentViolation(parentViolation) {}
    virtual ~ViolationContext() {}
};

template <typename T>
inline std::ostream& operator<<(std::ostream& os, const ExprRef<T>& ref) {
    return prettyPrint(os, ref->view());
}

HashType getValueHash(const AnyExprRef& ref);

inline bool appearsDefined(const AnyExprRef& expr) {
    return lib::visit([&](auto& expr) { return expr->appearsDefined(); }, expr);
}
std::ostream& prettyPrint(std::ostream& os, const AnyExprRef& expr);
inline std::ostream& operator<<(std::ostream& os, const AnyExprRef& expr) {
    return prettyPrint(os, expr);
}
template <typename View>
inline std::ostream& prettyPrint(std::ostream& os,
                                 const OptionalRef<View>& ref) {
    if (ref) {
        return prettyPrint(os, *ref);
    } else {
        return os << "undefined";
    }
}

template <typename View>
inline std::ostream& prettyPrint(
    std::ostream& os,
    const typename AssociatedDomain<
        typename AssociatedValueType<View>::type>::type& domain,
    const OptionalRef<View>& ref) {
    if (ref) {
        return prettyPrint(os, domain, *ref);
    } else {
        return os << "undefined";
    }
}

template <typename View>
inline std::ostream& operator<<(std::ostream& os,
                                const OptionalRef<View>& ref) {
    return prettyPrint(os, ref);
}

bool smallerValue(const AnyExprRef& u, const AnyExprRef& v);

bool largerValue(const AnyExprRef& u, const AnyExprRef& v);
bool equalValue(const AnyExprRef& u, const AnyExprRef& v);

template <typename T>
inline ExprRef<T> findAndReplace(ExprRef<T>& expr,
                                 const FindAndReplaceFunction& func,
                                 PathExtension& path,
                                 bool isCondition = false) {
    auto newExpr = func(expr, path);
    if (newExpr.first) {
        return lib::get<ExprRef<T>>(newExpr.second);
    } else {
        expr->findAndReplaceSelf(func, path.extend(expr, isCondition));
        return expr;
    }
}

template <typename T>
inline ExprRef<T> findAndReplace(ExprRef<T>& expr,
                                 const FindAndReplaceFunction& func) {
    auto path = PathExtension::begin(false);
    return findAndReplace(expr, func, path, false);
}
template <typename Op, typename View>
OptionalRef<Op> getAs(ExprRef<View>& expr) {
    Op* opTest = dynamic_cast<Op*>(&(*(expr)));
    if (opTest) {
        return *opTest;
    } else {
        return EmptyOptional();
    }
}

template <typename Op, typename View>
OptionalRef<const Op> getAs(const ExprRef<View>& expr) {
    const Op* opTest = dynamic_cast<const Op*>(&(*(expr)));
    if (opTest) {
        return *opTest;
    } else {
        return EmptyOptional();
    }
}

template <typename Op, typename View>
OptionalRef<Op> getAs(ExprRef<View>&& expr);

template <typename MemberExprType>
inline void recurseSanityChecks(const ExprRefVec<MemberExprType>& members) {
    for (auto& member : members) {
        member->debugSanityCheck();
    }
}

extern UInt LARGE_VIOLATION;
extern BoolView& VIOLATING_BOOL_VIEW;
extern UInt MAX_DOMAIN_SIZE;
#endif /* SRC_BASE_EXPRREF_H_ */
