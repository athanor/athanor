#include "types/bool.h"
#include <cassert>
#include "common/common.h"
using namespace std;

u_int64_t getValueHash(const BoolValue& val) { return val.value; }

ostream& prettyPrint(ostream& os, const BoolValue& v) {
    os << v.value;
    return os;
}

void deepCopy(const BoolValue& src, BoolValue& target) { target = src; }
ostream& prettyPrint(ostream& os, const BoolDomain&) {
    os << "bool";
    return os;
}

u_int64_t VIOLATION_1 = 1;
u_int64_t VIOLATION_0 = 0;
BoolView getBoolView(BoolValue& val) {
    return BoolView((val.value) ? VIOLATION_0 : VIOLATION_1, val.triggers);
}

void matchInnerType(const BoolValue&, BoolValue&) {}
void matchInnerType(const BoolDomain&, BoolValue&) {}

u_int64_t getDomainSize(const BoolDomain&) { return 2; }
