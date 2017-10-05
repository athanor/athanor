#include "types/set.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/forwardDecls/changeValue.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/hash.h"
#include "types/forwardDecls/print.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
using namespace std;

u_int64_t getValueHash(const SetValue& val) {
    return mpark::visit([](auto& valImpl) { return valImpl.cachedHashTotal; },
                        val.setValueImpl);
}

ostream& prettyPrint(ostream& os, const SetValue& v) {
    os << "set({";
    mpark::visit(
        [&](auto& vImpl) {
            bool first = true;
            for (auto& memberPtr : vImpl.members) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                prettyPrint(os, *memberPtr);
            }
        },
        v.setValueImpl);
    os << "})";
    return os;
}

template <typename InnerValuePtrType>
void deepCopyImpl(const SetValueImpl<InnerValuePtrType>& srcImpl,
                  SetValue& target) {
    auto& targetImpl =
        target.setValueImpl.emplace<SetValueImpl<InnerValuePtrType>>();
    targetImpl.memberHashes = srcImpl.memberHashes;
    targetImpl.cachedHashTotal = srcImpl.cachedHashTotal;
    targetImpl.members.reserve(srcImpl.members.size());
    transform(srcImpl.members.begin(), srcImpl.members.end(),
              back_inserter(targetImpl.members),
              [](auto& srcMemberPtr) { return deepCopy(*srcMemberPtr); });
}

void deepCopy(const SetValue& src, SetValue& target) {
    return visit([&](auto& srcImpl) { return deepCopyImpl(srcImpl, target); },
                 src.setValueImpl);
}

ostream& prettyPrint(ostream& os, const SetDomain& d) {
    os << "set(";
    os << d.sizeAttr << ",";
    prettyPrint(os, d.inner);
    os << ")";
    return os;
}

vector<shared_ptr<SetTrigger>>& getTriggers(SetValue& v) { return v.triggers; }

SetView getSetView(SetValue& val) {
    return mpark::visit(
        [&](auto& valImpl) {
            return SetView(valImpl.memberHashes, valImpl.cachedHashTotal,
                           val.triggers);
        },
        val.setValueImpl);
}
