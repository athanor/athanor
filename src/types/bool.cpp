#include "types/bool.h"
#include <cassert>
#include "common/common.h"
using namespace std;

u_int64_t getValueHash(const BoolValue& val) { return val.violation; }

ostream& prettyPrint(ostream& os, const BoolValue& v) {
    os << (v.violation == 1);
    return os;
}

void deepCopy(const BoolValue& src, BoolValue& target) {
    target.changeValue([&]() { target.violation = src.violation; });
}

ostream& prettyPrint(ostream& os, const BoolDomain&) {
    os << "bool";
    return os;
}

u_int64_t VIOLATION_1 = 1;
u_int64_t VIOLATION_0 = 0;

void matchInnerType(const BoolValue&, BoolValue&) {}
void matchInnerType(const BoolDomain&, BoolValue&) {}

u_int64_t getDomainSize(const BoolDomain&) { return 2; }

void reset(BoolValue& val) { val.triggers.clear(); }

void evaluate(BoolValue&) {}
void startTriggering(BoolValue&) {}
void stopTriggering(BoolValue&) {}

void normalise(BoolValue&) {}

bool smallerValue(const BoolValue& u, const BoolValue& v) {
    return u.violation < v.violation;
}

bool largerValue(const BoolValue& u, const BoolValue& v) {
    return u.violation > v.violation;
}

bool equalValue(const BoolValue& u, const BoolValue& v) {
    return u.violation == v.violation;
}
