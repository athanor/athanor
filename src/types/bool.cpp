#include "types/bool.h"
#include <cassert>
#include "common/common.h"
#include "base/typeOperations.h"

using namespace std;
template <>
u_int64_t getValueHash<BoolView>(const BoolView& val) {
    return val.violation;
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

u_int64_t VIOLATION_1 = 1;
u_int64_t VIOLATION_0 = 0;

void matchInnerType(const BoolValue&, BoolValue&) {}
void matchInnerType(const BoolDomain&, BoolValue&) {}

template <>
u_int64_t getDomainSize<BoolDomain>(const BoolDomain&) {
    return 2;
}

void reset(BoolValue& val) {
    val.triggers.clear();
    val.container = NULL;
}

void evaluate(BoolValue&) {}
void startTriggering(BoolValue&) {}
void stopTriggering(BoolValue&) {}

template <>
void normalise<BoolValue>(BoolValue&) {}

template <>
bool smallerValue<BoolValue>(const BoolValue& u, const BoolValue& v) {
    return u.violation < v.violation;
}

template <>
bool largerValue<BoolValue>(const BoolValue& u, const BoolValue& v) {
    return u.violation > v.violation;
}

template <>
bool equalValue<BoolValue>(const BoolValue& u, const BoolValue& v) {
    return u.violation == v.violation;
}
