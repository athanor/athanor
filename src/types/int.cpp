#include "types/int.h"
#include <cassert>
#include "common/common.h"
using namespace std;

void assignInitialValueInDomain(const IntDomain& domain, IntValue& val) {
    debug_code(assert(domain.bounds.size() > 0));
    val.containingBoundIndex = 0;
    val.value = domain.bounds[0].first;
    val.state = ValueState::DIRTY;
}

std::shared_ptr<IntValue> makeInitialValueInDomain(const IntDomain& domain) {
    auto val = make_shared<IntValue>();
    assignInitialValueInDomain(domain, *val);
    return val;
}
bool moveToNextValueInDomain(IntValue& val, const IntDomain& domain,
                             const ParentCheck& parentCheck) {
    while (true) {
        debug_code(assert(val.containingBoundIndex >= 0 &&
                          val.containingBoundIndex < domain.bounds.size()));
        if (val.value < domain.bounds[val.containingBoundIndex].second) {
            ++val.value;
        } else if (val.containingBoundIndex < domain.bounds.size() - 1) {
            ++val.containingBoundIndex;
            val.value = domain.bounds[val.containingBoundIndex].first;
        } else {
            val.state = ValueState::UNDEFINED;
            return false;
        }
        if (parentCheck()) {
            val.state = ValueState::DIRTY;
            return true;
        }
    }
}

u_int64_t getHash(const IntValue& val) { return val.value; }

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
