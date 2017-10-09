#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/forwardDecls/constructValue.h"
#include "types/set.h"
template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const SetDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   SetValue& val) {
    typedef std::shared_ptr<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValuePtrType;
    auto& valImpl =
        mpark::get<SetValueImpl<InnerValuePtrType>>(val.setValueImpl);
    u_int64_t sizeRange =
        (domain.sizeAttr.maxSize - domain.sizeAttr.minSize) + 1;
    // randomly choose a new size, todo, this to be improved
    size_t newNumberElements =
        domain.sizeAttr.minSize + (std::rand() % sizeRange);
    // clear set and populate with new random elements
    // reuse memory for existing members instead of dealocating them
    std::vector<InnerValuePtrType> newMembers = valImpl.removeAllValues(val);
    // remove members that are not needed
    if (newNumberElements < newMembers.size()) {
        newMembers.erase(newMembers.begin() + newNumberElements,
                         newMembers.end());
    }
    // otherwise, if not enough elements, add more
    newMembers.reserve(newNumberElements);
    while (newNumberElements > newMembers.size()) {
        newMembers.push_back(
            construct<typename InnerValuePtrType::element_type>());
        matchInnerType(*innerDomainPtr, *newMembers.back());
    }
    // now randomly assign elements and add to the set
    while (!newMembers.empty()) {
        do {
            assignRandomValueInDomain(*innerDomainPtr, *newMembers.back());
        } while (valImpl.addValue(val, newMembers.back()));
        newMembers.pop_back();
    }
}
void assignRandomValueInDomain(const SetDomain& domain, SetValue& val) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val);
        },
        domain.inner);
}
