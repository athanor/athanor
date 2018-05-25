
/*contains definitions for functions that use the same code for every type,
 * using macros to do the specialisation to avoid having to include this file,
 * which includes all the types */
#include <utility>
#include "search/violationContainer.h"
#include "types/allTypes.h"
#define quote(x) #x
using namespace std;
bool currentlyProcessingDelayedTriggerStack = false;
vector<shared_ptr<DelayedTrigger>> delayedTriggerStack;

inline pair<bool, ViolationContainer&> registerViolations(
    const ValBase* val, const UInt violation, ViolationContainer& vioDesc);
#define specialised(name)                                                  \
    template <>                                                            \
    ValRef<name##Value> make<name##Value>() {                              \
        ValRef<name##Value> val(make_shared<name##Value>());               \
        val->evaluated = true;                                             \
        return val;                                                        \
    }                                                                      \
    const string TypeAsString<name##Value>::value = quote(name##Value);    \
    const string TypeAsString<name##Domain>::value = quote(name##Domain);  \
    template <>                                                            \
    ValBase& valBase<name##Value>(name##Value & v) {                       \
        return v;                                                          \
    }                                                                      \
    template <>                                                            \
    const ValBase& valBase<name##Value>(const name##Value& v) {            \
        return v;                                                          \
    }                                                                      \
                                                                           \
    void name##Value::evaluateImpl() {}                                    \
    void name##Value::startTriggering() {}                                 \
    void name##Value::stopTriggering() {}                                  \
    void name##Value::updateVarViolations(                          \
        UInt parentViolation, ViolationContainer& vioDesc) {             \
        registerViolations(this, parentViolation, vioDesc);                \
    }                                                                      \
    ExprRef<name##View> name##Value::deepCopySelfForUnroll(                \
        const ExprRef<name##View>& self, const AnyIterRef&) const {        \
        return self;                                                       \
    }                                                                      \
    std::ostream& name##Value::dumpState(std::ostream& os) const {         \
        return prettyPrint(os, *static_cast<const name##View*>(this));     \
    }                                                                      \
                                                                           \
    void name##Value::findAndReplaceSelf(const FindAndReplaceFunction&) {} \
    bool name##Value::isUndefined() { return false; }                      \
    bool name##Value::optimise(PathExtension) { return false; }

buildForAllTypes(specialised, )
#undef specialised

    vector<shared_ptr<DelayedTrigger>> emptyEndOfTriggerQueue;

inline pair<bool, ViolationContainer&> registerViolations(
    const ValBase* val, const UInt violation, ViolationContainer& vioDesc) {
    if (val->container == &constantPool) {
        return pair<bool, ViolationContainer&>(false, vioDesc);
    }
    if (val->container == NULL) {
        vioDesc.addViolation(val->id, violation);
        return pair<bool, ViolationContainer&>(true, vioDesc);
    } else {
        auto boolVioDescPair =
            registerViolations(val->container, violation, vioDesc);
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
