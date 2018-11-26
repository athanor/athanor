#include "types/bool.h"
#include <cassert>
#include "common/common.h"

using namespace std;
template <>
HashType getValueHash<BoolView>(const BoolView& val) {
    return val.violation == 0;
}

template <>
ostream& prettyPrint<BoolView>(ostream& os, const BoolView& v) {
    os << (v.violation == 1);
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
bool smallerValue<BoolView>(const BoolView& u, const BoolView& v) {
    return u.violation < v.violation;
}

template <>
bool largerValue<BoolView>(const BoolView& u, const BoolView& v) {
    return u.violation > v.violation;
}
