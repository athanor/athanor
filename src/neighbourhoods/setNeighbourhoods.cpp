#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/set.h"
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
    u_int64_t sizeRange =
        (domain.sizeAttr.maxSize - domain.sizeAttr.minSize) + 1;
    // randomly choose a new size, todo, this to be improved
    size_t newNumberElements = domain.sizeAttr.minSize + (rand() % sizeRange);
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
