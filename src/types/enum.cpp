#include <cassert>
#include "common/common.h"
#include "types/enumVal.h"
using namespace std;

template <>
HashType getValueHash<EnumView>(const EnumView& val) {
    return HashType(val.value);
}

template <>
ostream& prettyPrint<EnumView>(ostream& os, const EnumView& v) {
    os << "enum(" << v.value << ")";
    return os;
}

template <>
ostream& prettyPrint<EnumView>(ostream& os, const EnumDomain& domain,
                               const EnumView& v) {
    domain.printValue(os, v.value);
    return os;
}

template <>
void deepCopy<EnumValue>(const EnumValue& src, EnumValue& target) {
    target.changeValue([&]() {
        target.value = src.value;
        return true;
    });
}

template <>
ostream& prettyPrint<EnumDomain>(ostream& os, const EnumDomain& d) {
    return os << d.domainName;
}

void matchInnerType(const EnumValue&, EnumValue&) {}
void matchInnerType(const EnumDomain&, EnumValue&) {}

template <>
UInt getDomainSize<EnumDomain>(const EnumDomain& domain) {
    return domain.numberValues();
}

void evaluateImpl(EnumValue&) {}
void startTriggeringImpl(EnumValue&) {}
void stopTriggering(EnumValue&) {}

template <>
bool smallerValue<EnumView>(const EnumView& u, const EnumView& v) {
    return u.value < v.value;
}

template <>
bool largerValue<EnumView>(const EnumView& u, const EnumView& v) {
    return u.value > v.value;
}

template <>
void normalise<EnumValue>(EnumValue&) {}

void EnumView::standardSanityChecksForThisType() const {}

void EnumValue::debugSanityCheckImpl() const {
    standardSanityChecksForThisType();
}

template <>
bool hasVariableSize<EnumValue>(const EnumValue&) {
    return false;
}
template <>
UInt getSize<EnumValue>(const EnumValue&) {
    return 0;
}

template <>
size_t getResourceLowerBound<EnumDomain>(const EnumDomain&) {
    return 1;
}
