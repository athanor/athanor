#include "types/bool.h"
#include <cassert>
#include "common/common.h"
#include "types/parentCheck.h"
using namespace std;

void assignInitialValueInDomain(const BoolDomain&, BoolValue& val) {
    val.value = false;
    val.state = ValueState::DIRTY;
}

std::shared_ptr<BoolValue> makeInitialValueInDomain(const BoolDomain& domain) {
    auto val = make_shared<BoolValue>();
    assignInitialValueInDomain(domain, *val);
    return val;
}
bool moveToNextValueInDomain(BoolValue& val, const BoolDomain&,
                             const ParentCheck& parentCheck) {
    while (true) {
        if (val.value == false) {
            val.value = true;
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

u_int64_t getHash(const BoolValue& val) { return val.value; }

ostream& prettyPrint(ostream& os, const BoolValue& v) {
    os << v.value;
    return os;
}

void deepCopy(const BoolValue& src, BoolValue& target) { target = src; }
ostream& prettyPrint(ostream& os, const BoolDomain&) {
    os << "bool";
    return os;
}
