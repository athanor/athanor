#include <cassert>

#include "common/common.h"
#include "types/emptyVal.h"
#include "utils/ignoreUnused.h"

using namespace std;

template <>
HashType getValueHash<EmptyView>(const EmptyView&) {
    shouldNotBeCalledPanic;
}

template <>
ostream& prettyPrint<EmptyView>(ostream&, const EmptyView&) {
    shouldNotBeCalledPanic;
}

template <>
ostream& prettyPrint<EmptyView>(ostream&, const EmptyDomain&,
                                const EmptyView&) {
    shouldNotBeCalledPanic;
}

template <>
void deepCopy<EmptyValue>(const EmptyValue&, EmptyValue&) {
    shouldNotBeCalledPanic;
}

template <>
ostream& prettyPrint<EmptyDomain>(ostream& os, const EmptyDomain&) {
    os << "empty";
    return os;
}

void matchInnerType(const EmptyValue&, EmptyValue&) { shouldNotBeCalledPanic; }

void matchInnerType(const EmptyDomain&, EmptyValue&) { shouldNotBeCalledPanic; }

template <>
UInt getDomainSize<EmptyDomain>(const EmptyDomain&) {
    return MAX_DOMAIN_SIZE;
}

template <>
void normalise<EmptyValue>(EmptyValue&) {
    shouldNotBeCalledPanic;
}

template <>
bool smallerValueImpl<EmptyView>(const EmptyView& u, const EmptyView& v);
template <>
bool largerValueImpl<EmptyView>(const EmptyView& u, const EmptyView& v);

template <>
bool smallerValueImpl<EmptyView>(const EmptyView&, const EmptyView&) {
    shouldNotBeCalledPanic;
}

template <>
bool largerValueImpl<EmptyView>(const EmptyView&, const EmptyView&) {
    shouldNotBeCalledPanic;
}

template <>
bool equalValueImpl<EmptyView>(const EmptyView&, const EmptyView&) {
    shouldNotBeCalledPanic;
}

void EmptyValue::debugSanityCheckImpl() const { shouldNotBeCalledPanic; }

template <>
bool hasVariableSize<EmptyValue>(const EmptyValue&) {
    shouldNotBeCalledPanic;
}
template <>
UInt getSize<EmptyValue>(const EmptyValue&) {
    shouldNotBeCalledPanic;
}

template <>
size_t getResourceLowerBound<EmptyDomain>(const EmptyDomain&) {
    shouldNotBeCalledPanic;
}
