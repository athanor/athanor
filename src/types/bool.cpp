#include "types/bool.h"
#include <cassert>
#include "common/common.h"
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
    bool success;
    val.changeValue([&]() {
        if (val.value == false) {
            val.value = true;
            if (parentCheck()) {
                success = true;
                val.state = ValueState::DIRTY;
                return;
            }
        }
        val.state = ValueState::UNDEFINED;
        success = false;
        return;
    });
    return success;
}

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

BoolView getBoolView(BoolValue& val) { return BoolView(val.triggers); }
