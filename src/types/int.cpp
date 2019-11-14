#include <cassert>
#include "common/common.h"
#include "types/intVal.h"
using namespace std;

template <>
HashType getValueHash<IntView>(const IntView& val) {
    return HashType(val.value);
}

template <>
ostream& prettyPrint<IntView>(ostream& os, const IntView& v) {
    os << v.value;
    return os;
}

template <>
ostream& prettyPrint<IntView>(ostream& os, const IntDomain&, const IntView& v) {
    os << v.value;
    return os;
}

template <>
void deepCopy<IntValue>(const IntValue& src, IntValue& target) {
    target.domain = src.domain;
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
UInt getDomainSize<IntDomain>(const IntDomain& domain) {
    return domain.domainSize;
}

void evaluateImpl(IntValue&) {}
void startTriggeringImpl(IntValue&) {}
void stopTriggering(IntValue&) {}

template <>
bool smallerValue<IntView>(const IntView& u, const IntView& v) {
    return u.value < v.value;
}

template <>
bool largerValue<IntView>(const IntView& u, const IntView& v) {
    return u.value > v.value;
}

template <>
void normalise<IntValue>(IntValue&) {}

void IntView::standardSanityChecksForThisType() const {}

void IntValue::debugSanityCheckImpl() const {
    standardSanityChecksForThisType();
}

template <>
bool hasVariableSize<IntValue>(const IntValue&) {
    return false;
}
template <>
UInt getSize<IntValue>(const IntValue&) {
    return 0;
}

template <>
size_t getResourceLowerBound<IntDomain>(const IntDomain&) {
    return 1;
}
