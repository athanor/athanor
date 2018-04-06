#include "base/valRef.h"
#include "utils/ignoreUnused.h"
using namespace std;

ValBase constantPool;
ValBase definedPool;

bool smallerValue(const AnyValRef& u, const AnyValRef& v) {
    return u.index() < v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return smallerValue(*uImpl, *vImpl);
                },
                u));
}

void normalise(AnyValRef& v) {
    mpark::visit([](auto& vImpl) { normalise(*vImpl); }, v);
}

bool largerValue(const AnyValRef& u, const AnyValRef& v) {
    return u.index() > v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return largerValue(*uImpl, *vImpl);
                },
                u));
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
