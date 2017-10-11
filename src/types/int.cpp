#include "types/int.h"
#include <cassert>
#include "common/common.h"
using namespace std;

u_int64_t getValueHash(const IntValue& val) { return val.value; }

ostream& prettyPrint(ostream& os, const IntValue& v) {
    os << v.value;
    return os;
}

void deepCopy(const IntValue& src, IntValue& target) { target = src; }

ostream& prettyPrint(ostream& os, const IntDomain& d) {
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

IntView getIntView(IntValue& val) { return IntView(val.value, val.triggers); }

void matchInnerType(const IntValue&, IntValue&) {}
void matchInnerType(const IntDomain&, IntValue&) {}

u_int64_t getDomainSize(const IntDomain& domain) { return domain.domainSize; }

void reset(IntValue& val) { val.triggers.clear(); }
