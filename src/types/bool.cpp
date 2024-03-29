#include <cassert>

#include "common/common.h"
#include "types/boolVal.h"

using namespace std;
template <>
HashType getValueHash<BoolView>(const BoolView& val) {
    return HashType(val.violation == 0);
}

template <>
ostream& prettyPrint<BoolView>(ostream& os, const BoolView& v) {
    os << ((v.violation == 0) ? "true" : "false");
    return os;
}

template <>
ostream& prettyPrint<BoolView>(ostream& os, const BoolDomain&,
                               const BoolView& v) {
    os << ((v.violation == 0) ? "true" : "false");
    return os;
}

template <>
void deepCopy<BoolValue>(const BoolValue& src, BoolValue& target) {
    target.changeValue([&]() {
        target.violation = src.violation;
        return true;
    });
}

template <>
ostream& prettyPrint<BoolDomain>(ostream& os, const BoolDomain&) {
    os << "bool";
    return os;
}

UInt VIOLATION_1 = 1;
UInt VIOLATION_0 = 0;

void matchInnerType(const BoolValue&, BoolValue&) {}
void matchInnerType(const BoolDomain&, BoolValue&) {}

template <>
UInt getDomainSize<BoolDomain>(const BoolDomain&) {
    return 2;
}

void evaluateImpl(BoolValue&) {}
void startTriggeringImpl(BoolValue&) {}
void stopTriggering(BoolValue&) {}

template <>
void normalise<BoolValue>(BoolValue&) {}

template <>
bool smallerValueImpl<BoolView>(const BoolView& u, const BoolView& v) {
    return u.violation < v.violation;
}

template <>
bool largerValueImpl<BoolView>(const BoolView& u, const BoolView& v) {
    return u.violation > v.violation;
}

template <>
bool equalValueImpl<BoolView>(const BoolView& u, const BoolView& v) {
    return u.violation == v.violation;
}

void BoolView::standardSanityChecksForThisType() const {}

void BoolValue::debugSanityCheckImpl() const {
    standardSanityChecksForThisType();
}

template <>
bool hasVariableSize<BoolValue>(const BoolValue&) {
    return false;
}
template <>
UInt getSize<BoolValue>(const BoolValue&) {
    return 0;
}

template <>
size_t getResourceLowerBound<BoolDomain>(const BoolDomain&) {
    return 1;
}
