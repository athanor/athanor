#include "operators/opUndefined.h"
#include "operators/emptyOrViolatingOptional.h"
#include "triggers/allTriggers.h"
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename View>
void OpUndefined<View>::addTriggerImpl(const shared_ptr<Trigger>&, bool, Int) {}

template <typename View>
OptionalRef<View> OpUndefined<View>::view() {
    return emptyOrViolatingOptional<View>();
}

template <typename View>
OptionalRef<const View> OpUndefined<View>::view() const {
    return emptyOrViolatingOptional<View>();
}
template <typename View>
void setUndefinedIfNotBool(OpUndefined<View>& expr) {
    expr.setAppearsDefined(false);
}
template <>
void setUndefinedIfNotBool<BoolView>(OpUndefined<BoolView>&) {}

template <typename View>
void OpUndefined<View>::evaluateImpl() {
    setUndefinedIfNotBool(*this);
}

template <typename View>
void OpUndefined<View>::startTriggeringImpl() {}

template <typename View>
void OpUndefined<View>::stopTriggering() {}

template <typename View>
void OpUndefined<View>::updateVarViolationsImpl(const ViolationContext&,
                                                ViolationContainer&) {}

template <typename View>
ExprRef<View> OpUndefined<View>::deepCopyForUnrollImpl(
    const ExprRef<View>& self, const AnyIterRef&) const {
    return self;
}

template <typename View>
std::ostream& OpUndefined<View>::dumpState(std::ostream& os) const {
    return os << getOpName();
}

template <typename View>
void OpUndefined<View>::findAndReplaceSelf(const FindAndReplaceFunction&) {}

template <typename View>
pair<bool, ExprRef<View>> OpUndefined<View>::optimise(PathExtension path) {
    return make_pair(false, mpark::get<ExprRef<View>>(path.expr));
}

template <typename View>
string OpUndefined<View>::getOpName() const {
    return toString(
        "OpUndefined<",
        TypeAsString<typename AssociatedValueType<View>::type>::value, ">");
}

template <typename View>
void OpUndefined<View>::debugSanityCheckImpl() const {
    sanityCheck((!is_same<BoolView, View>::value || this->appearsDefined()),
                "This should be defined as it is a bool type.");
    sanityCheck((is_same<BoolView, View>::value || !this->appearsDefined()),
                "This should be undefined.");
}

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpUndefined<View>> {
    static ExprRef<View> make();
};

template <typename View>
ExprRef<View> OpMaker<OpUndefined<View>>::make() {
    return make_shared<OpUndefined<View>>();
}

#define opUndefinedInstantiators(name)       \
    template struct OpUndefined<name##View>; \
    template struct OpMaker<OpUndefined<name##View>>;

 buildForAllTypes(opUndefinedInstantiators, );
#undef opUndefinedInstantiators
