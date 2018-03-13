#include "types/int.h"
#include <cassert>
#include "common/common.h"
#include "base/typeOperations.h"
using namespace std;

template <>
u_int64_t getValueHash<IntView>(const IntView& val) {
    return val.value;
}

template <>
ostream& prettyPrint<IntView>(ostream& os, const IntView& v) {
    os << v.value;
    return os;
}

template <>
void deepCopy<IntValue>(const IntValue& src, IntValue& target) {
    target.changeValue([&]() {
        target.value = src.value;
        return true;
    });
}

template <>
ostream& prettyPrint<IntDomain>(ostream& os, const IntDomain& d) {
    os << "int(";
    bool first = true;
    for (auto& bound : d.bounds) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        if (bound.first == bound.second) {
            os << bound.first;
        } else {
            os << bound.first << ".." << bound.second;
        }
    }
    os << ")";
    return os;
}

void matchInnerType(const IntValue&, IntValue&) {}
void matchInnerType(const IntDomain&, IntValue&) {}

template <>
u_int64_t getDomainSize<IntDomain>(const IntDomain& domain) {
    return domain.domainSize;
}

void reset(IntValue& val) {
    val.triggers.clear();
    val.container = NULL;
}

void evaluate(IntValue&) {}
void startTriggering(IntValue&) {}
void stopTriggering(IntValue&) {}

template <>
bool smallerValue<IntValue>(const IntValue& u, const IntValue& v) {
    return u.value < v.value;
}

template <>
bool largerValue<IntValue>(const IntValue& u, const IntValue& v) {
    return u.value > v.value;
}

template <>
bool equalValue<IntValue>(const IntValue& u, const IntValue& v) {
    return u.value == v.value;
}

template <>
void normalise<IntValue>(IntValue&) {}
