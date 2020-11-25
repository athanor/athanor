
/*contains definitions for functions that use the same code for every type,
 * using macros to do the specialisation to avoid having to include this file,
 * which includes all the types */
#include <utility>
#include "search/violationContainer.h"
#include "types/allVals.h"
#define quote(x) #x
using namespace std;
template <typename Val>
void swapValAssignments(Val& val1, Val& val2);

inline pair<bool, ViolationContainer&> registerViolations(
    const ValBase* val, const UInt violation, ViolationContainer& vioContainer);
namespace {
template <typename Value>
void invokeSetDefined(ValRef<Value>& val) {
    val->setAppearsDefined(true);
}
template <>
void invokeSetDefined<BoolValue>(ValRef<BoolValue>&) {}
}  // namespace
#define specialised(name)                                                   \
    template <>                                                             \
    ValRef<name##Value> make<name##Value>() {                               \
        ValRef<name##Value> val(make_shared<name##Value>());                \
        val->setEvaluated(true);                                            \
        invokeSetDefined(val);                                              \
        return val;                                                         \
    }                                                                       \
    const string TypeAsString<name##Value>::value = quote(name##Value);     \
    const string TypeAsString<name##Domain>::value = quote(name##Domain);   \
    template <>                                                             \
    ValBase& valBase<name##Value>(name##Value & v) {                        \
        return v;                                                           \
    }                                                                       \
    template <>                                                             \
    const ValBase& valBase<name##Value>(const name##Value& v) {             \
        return v;                                                           \
    }                                                                       \
                                                                            \
    void name##Value::evaluateImpl() {}                                     \
    void name##Value::startTriggeringImpl() {}                              \
    void name##Value::stopTriggering() {}                                   \
    void name##Value::updateVarViolationsImpl(                              \
        const ViolationContext& vioContext,                                 \
        ViolationContainer& vioContainer) {                                 \
        registerViolations(this, vioContext.parentViolation, vioContainer); \
    }                                                                       \
    ExprRef<name##View> name##Value::deepCopyForUnrollImpl(                 \
        const ExprRef<name##View>& self, const AnyIterRef&) const {         \
        return self;                                                        \
    }                                                                       \
    std::ostream& name##Value::dumpState(std::ostream& os) const {          \
        return prettyPrint(os, *static_cast<const name##View*>(this));      \
    }                                                                       \
                                                                            \
    void name##Value::findAndReplaceSelf(const FindAndReplaceFunction&,     \
                                         PathExtension) {}                  \
    pair<bool, ExprRef<name##View>> name##Value::optimiseImpl(              \
        ExprRef<name##View>& self, PathExtension) {                         \
        return make_pair(false, self);                                      \
    }                                                                       \
    string name##Value::getOpName() const {                                 \
        return TypeAsString<name##Value>::value;                            \
    }                                                                       \
    void name##Value::hashChecksImpl() const {}                             \
    template <>                                                             \
    void swapValAssignments<name##Value>(name##Value & val1,                \
                                         name##Value & val2) {              \
        name##Value temp;                                                   \
        matchInnerType(val1, temp);                                         \
        deepCopy(val1, temp);                                               \
        deepCopy(val2, val1);                                               \
        deepCopy(temp, val2);                                               \
    }

buildForAllTypes(specialised, )
#undef specialised

    inline pair<bool, ViolationContainer&> registerViolations(
        const ValBase* val, const UInt violation,
        ViolationContainer& vioContainer) {
    if (val->container == &variablePool) {
        vioContainer.addViolation(val->id, violation);
        return pair<bool, ViolationContainer&>(true, vioContainer);

    } else if (!val->container || isPoolMarker(val->container)) {
        return pair<bool, ViolationContainer&>(false, vioContainer);
    } else {
        auto boolVioDescPair =
            registerViolations(val->container, violation, vioContainer);
        ViolationContainer& parentVioDesc = boolVioDescPair.second;
        if (boolVioDescPair.first) {
            ViolationContainer& childVioDesc =
                parentVioDesc.childViolations(val->container->id);
            childVioDesc.addViolation(val->id, violation);
            return pair<bool, ViolationContainer&>(true, childVioDesc);
        } else {
            return boolVioDescPair;
        }
    }
}
