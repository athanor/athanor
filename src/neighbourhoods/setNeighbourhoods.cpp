#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/set.h"
#include "types/typeOperations.h"
#include "utils/random.h"

using namespace std;

template <>
void assignRandomValueInDomain<SetDomain>(const SetDomain& domain,
                                          SetValue& val);
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
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        do {
            assignRandomValueInDomain(*innerDomainPtr, *newMember);
        } while (!valImpl.addValue(val, newMember));
    }
}

template <>
void assignRandomValueInDomain<SetDomain>(const SetDomain& domain,
                                          SetValue& val) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val);
        },
        domain.inner);
}

const int NUMBER_TRIES_CONSTANT_MULTIPLIER = 2;
int getTryLimit(u_int64_t numberMembers, u_int64_t domainSize) {
    double successChance = (domainSize - numberMembers) / (double)domainSize;
    return (int)(ceil(NUMBER_TRIES_CONSTANT_MULTIPLIER / successChance));
}

void setAddGen(const SetDomain& domain,
               std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    u_int64_t innerDomainSize = getDomainSize(domain.inner);
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            typedef BaseType<decltype(innerDomainPtr)> InnerDomainPtrType;
            typedef ValRef<typename AssociatedValueType<
                typename InnerDomainPtrType::element_type>::type>
                InnerValueRefType;
            neighbourhoods.emplace_back(
                "setAdd", [innerDomainSize, &domain,
                           &innerDomainPtr](NeighbourhoodParams& params) {
                    auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
                    auto& valImpl = mpark::get<SetValueImpl<InnerValueRefType>>(
                        val.setValueImpl);
                    if (valImpl.members.size() == domain.sizeAttr.maxSize) {
                        ++params.stats.minorNodeCount;
                        return;
                    }
                    auto newMember = constructValueFromDomain(*innerDomainPtr);
                    int numberTries = 0;
                    const int tryLimit =
                        getTryLimit(valImpl.members.size(), innerDomainSize);
                    debug_neighbourhood_action("Looking for value to add");
                    do {
                        ++params.stats.minorNodeCount;
                        assignRandomValueInDomain(*innerDomainPtr, *newMember);
                    } while (!valImpl.addValue(val, newMember) &&
                             ++numberTries < tryLimit);
                    if (numberTries >= tryLimit) {
                        debug_neighbourhood_action(
                            "Couldn't find value, number tries=" << tryLimit);
                        return;
                    }
                    debug_neighbourhood_action("Added value: " << *newMember);
                    if (!params.changeAccepted()) {
                        debug_neighbourhood_action("Change rejected"
                                                   << tryLimit);
                        valImpl.removeValue(val, valImpl.members.size() - 1);
                    }
                });
        },
        domain.inner);
}

void setRemoveGen(const SetDomain& domain,
                  std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            typedef BaseType<decltype(innerDomainPtr)> InnerDomainPtrType;
            typedef ValRef<typename AssociatedValueType<
                typename InnerDomainPtrType::element_type>::type>
                InnerValueRefType;
            neighbourhoods.emplace_back(
                "setRemove", [&](NeighbourhoodParams& params) {
                    ++params.stats.minorNodeCount;
                    auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
                    auto& valImpl = mpark::get<SetValueImpl<InnerValueRefType>>(
                        val.setValueImpl);
                    if (valImpl.members.size() == domain.sizeAttr.minSize) {
                        return;
                    }
                    size_t indexToRemove =
                        globalRandom<size_t>(0, valImpl.members.size() - 1);
                    auto removedMember =
                        valImpl.removeValue(val, indexToRemove);
                    debug_neighbourhood_action("Removed " << *removedMember);

                    if (!params.changeAccepted()) {
                        debug_neighbourhood_action("Change rejected");
                        valImpl.addValue(val, std::move(removedMember));
                    }
                });
        },
        domain.inner);
}

void setSwapGen(const SetDomain& domain,
                std::vector<Neighbourhood>& neighbourhoods) {
    u_int64_t innerDomainSize = getDomainSize(domain.inner);
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            typedef BaseType<decltype(innerDomainPtr)> InnerDomainPtrType;
            typedef ValRef<typename AssociatedValueType<
                typename InnerDomainPtrType::element_type>::type>
                InnerValueRefType;
            neighbourhoods.emplace_back(
                "setSwap", [innerDomainSize, &domain,
                            &innerDomainPtr](NeighbourhoodParams& params) {
                    auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
                    auto& valImpl = mpark::get<SetValueImpl<InnerValueRefType>>(
                        val.setValueImpl);
                    if (valImpl.members.size() == 0) {
                        ++params.stats.minorNodeCount;
                        return;
                    }
                    const int tryLimit =
                        getTryLimit(valImpl.members.size(), innerDomainSize);
                    int numberTries = 0;
                    size_t indexToChange =
                        globalRandom<size_t>(0, valImpl.members.size() - 1);
                    bool success = false;
                    auto memberBackup =
                        deepCopy(*valImpl.members[indexToChange]);
                    debug_neighbourhood_action("Changing value "
                                               << *memberBackup);
                    auto newMember = constructValueFromDomain(*innerDomainPtr);
                    do {
                        ++params.stats.minorNodeCount;
                        assignRandomValueInDomain(*innerDomainPtr, *newMember);
                        success = !val.containsMember(newMember);
                    } while (!success && ++numberTries < tryLimit);
                    if (!success) {
                        debug_neighbourhood_action(
                            "Couldn't find new value, number tries="
                            << tryLimit);
                        return;
                    }
                    debug_neighbourhood_action("New Value: " << *newMember);
                    valImpl.changeMemberValue(
                        [&]() {
                            deepCopy(*newMember,
                                     *valImpl.members[indexToChange]);
                            return true;
                        },
                        val, indexToChange);
                    if (!params.changeAccepted()) {
                        debug_neighbourhood_action("Change rejected");
                        valImpl.changeMemberValue(
                            [&]() {
                                deepCopy(*memberBackup,
                                         *valImpl.members[indexToChange]);
                                return true;
                            },
                            val, indexToChange);
                    }
                });
        },
        domain.inner);
}

const NeighbourhoodGenList<SetDomain>::type
    NeighbourhoodGenList<SetDomain>::value = {setAddGen, setRemoveGen,
                                              setSwapGen};
