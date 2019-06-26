#include "base/valRef.h"
#include "utils/ignoreUnused.h"
using namespace std;

ValBase constantPool;
ValBase inlinedPool;
ValBase variablePool;
// checks if v is one of the global pools like inlinedPool or variablePool for
// example.
bool isPoolMarker(const ValBase* v) {
    return v == &variablePool || v == &inlinedPool || v == &constantPool;
}

void normalise(AnyValRef& v) {
    mpark::visit([](auto& vImpl) { normalise(*vImpl); }, v);
}
AnyValRef deepCopy(const AnyValRef& val) {
    return mpark::visit(
        [](const auto& valImpl) { return AnyValRef(deepCopy(*valImpl)); }, val);
}

ostream& prettyPrint(ostream& os, const AnyValRef& v) {
    return mpark::visit(
        [&os](auto& vImpl) -> ostream& { return prettyPrint(os, vImpl); }, v);
}

HashType getValueHash(const AnyValRef& v) {
    return mpark::visit([](auto& vImpl) { return getValueHash(vImpl); }, v);
}

ostream& operator<<(ostream& os, const AnyValRef& v) {
    return prettyPrint(os, v);
}

const ValBase& valBase(const AnyValRef& ref) {
    return mpark::visit(
        [](auto& val) -> const ValBase& { return valBase(*val); }, ref);
}
ValBase& valBase(AnyValRef& ref) {
    return mpark::visit([](auto& val) -> ValBase& { return valBase(*val); },
                        ref);
}
