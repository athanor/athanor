#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/set.h"
#include "utils/random.h"
using namespace std;
template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const SetDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   SetValue& val) {
    typedef ValRef<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValueRefType;
    auto& valImpl =
        mpark::get<SetValueImpl<InnerValueRefType>>(val.setValueImpl);
    size_t newNumberElements =
        globalRandom(domain.sizeAttr.minSize, domain.sizeAttr.maxSize);
    // clear set and populate with new random elements
    valImpl.removeAllValues(val);
    while (newNumberElements > valImpl.members.size()) {
        auto newMember = construct<typename InnerValueRefType::element_type>();
        matchInnerType(*innerDomainPtr, *newMember);
        do {
            assignRandomValueInDomain(*innerDomainPtr, *newMember);
        } while (!valImpl.addValue(val, newMember));
    }
}
void assignRandomValueInDomain(const SetDomain& domain, SetValue& val) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val);
        },
        domain.inner);
}
const NeighbourhoodGenList<SetDomain>::type
    NeighbourhoodGenList<SetDomain>::value = {};
