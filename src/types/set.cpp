#include "types/set.h"
#include <algorithm>
#include <cassert>
#include "common/common.h"
#include "types/forwardDecls/copy.h"
#include "types/forwardDecls/getDomainSize.h"
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

template <typename InnerValueRefType>
void deepCopyImpl(const SetValueImpl<InnerValueRefType>& srcImpl,
                  SetValue& target) {
    auto& targetImpl =
        mpark::get<SetValueImpl<InnerValueRefType>>(target.setValueImpl);
    // to be optimised later
    targetImpl.removeAllValues(target);
    for (auto& member : srcImpl.members) {
        targetImpl.addValue(target, deepCopy(*member));
    }
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

template <typename InnerValueRefType>
void matchInnerTypeImpl(const SetValueImpl<InnerValueRefType>&,
                        SetValue& target) {
    if (mpark::get_if<SetValueImpl<InnerValueRefType>>(
            &(target.setValueImpl)) == NULL) {
        target.setValueImpl = SetValueImpl<InnerValueRefType>();
    }
}

void matchInnerType(const SetValue& src, SetValue& target) {
    mpark::visit([&](auto& srcImpl) { matchInnerTypeImpl(srcImpl, target); },
                 src.setValueImpl);
}

template <typename InnerDomainPtrType>
void matchInnerTypeFromDomain(const InnerDomainPtrType&, SetValue& target) {
    typedef ValRef<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValueRefType;
    if (mpark::get_if<SetValueImpl<InnerValueRefType>>(
            &(target.setValueImpl)) == NULL) {
        target.setValueImpl = SetValueImpl<InnerValueRefType>();
    }
}

void matchInnerType(const SetDomain& domain, SetValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            matchInnerTypeFromDomain(innerDomainImpl, target);
        },
        domain.inner);
}

u_int64_t getDomainSize(const SetDomain& domain) {
    return 1 << getDomainSize(domain.inner);
}

template <typename InnerValueRefType>
void resetImpl(SetValue& val, SetValueImpl<InnerValueRefType>& valImpl) {
    val.triggers.clear();
    valImpl.members.clear();
    valImpl.memberHashes.clear();
    valImpl.cachedHashTotal = 0;
}
void reset(SetValue& val) {
    mpark::visit([&](auto& valImpl) { resetImpl(val, valImpl); },
                 val.setValueImpl);
}

void evaluate(SetValue&) {}
void startTriggering(SetValue&) {}
void stopTriggering(SetValue&) {}
