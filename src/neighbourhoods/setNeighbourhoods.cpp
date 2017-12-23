#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/set.h"
#include "types/typeOperations.h"
#include "utils/random.h"

using namespace std;
ViolationDescription emptyViolations;

template <>
void assignRandomValueInDomain<SetDomain>(const SetDomain& domain,
                                          SetValue& val);
template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const SetDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   SetValue& val) {
    typedef ValRef<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValRefType;
    auto& valImpl = mpark::get<SetValueImpl<InnerValRefType>>(val.setValueImpl);
    size_t newNumberElements =
        globalRandom(domain.sizeAttr.minSize, domain.sizeAttr.maxSize);
    newNumberElements = 2;
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
template <typename InnerDomainPtrType>
void setAddGenImpl(const SetDomain& domain, InnerDomainPtrType& innerDomainPtr,
                   std::vector<Neighbourhood>& neighbourhoods) {
    typedef ValRef<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValRefType;
    u_int64_t innerDomainSize = getDomainSize(domain.inner);

    neighbourhoods.emplace_back("setAdd", [innerDomainSize, &domain,
                                           &innerDomainPtr](
                                              NeighbourhoodParams& params) {
        auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
        auto& valImpl =
            mpark::get<SetValueImpl<InnerValRefType>>(val.setValueImpl);
        if (valImpl.members.size() == domain.sizeAttr.maxSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        int numberTries = 0;
        const int tryLimit =
            params.parentCheckTryLimit *
            getTryLimit(valImpl.members.size(), innerDomainSize);
        debug_neighbourhood_action("Looking for value to add");
        bool success;
        do {
            ++params.stats.minorNodeCount;
            assignRandomValueInDomain(*innerDomainPtr, *newMember);
            success = false;
            if (valImpl.addValueSilent(val, newMember)) {
                if (params.parentCheck(params.primary)) {
                    success = true;
                } else {
                    valImpl.removeValueSilent(val, valImpl.members.size() - 1);
                }
            }
            if (!success && ++numberTries >= tryLimit) {
                break;
            }
        } while (!success);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find value, number tries=" << tryLimit);
            return;
        }
        debug_neighbourhood_action("Added value: " << *newMember);
        val.signalValueAdded(newMember);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            valImpl.removeValue(val, valImpl.members.size() - 1);
        }
    });
}
void setAddGen(const SetDomain& domain,
               std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setAddGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setRemoveGenImpl(const SetDomain& domain, InnerDomainPtrType&,
                      std::vector<Neighbourhood>& neighbourhoods) {
    typedef ValRef<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValRefType;
    neighbourhoods.emplace_back("setRemove", [&](NeighbourhoodParams& params) {
        ++params.stats.minorNodeCount;
        auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
        auto& valImpl =
            mpark::get<SetValueImpl<InnerValRefType>>(val.setValueImpl);
        if (valImpl.members.size() == domain.sizeAttr.minSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        size_t indexToRemove;
        InnerValRefType removedMember(nullptr);
        int numberTries = 0;

        bool success;
        debug_neighbourhood_action("Looking for value to remove");
        do {
            indexToRemove = globalRandom<size_t>(0, valImpl.members.size() - 1);
            removedMember =
                std::move(valImpl.removeValueSilent(val, indexToRemove));
            success = params.parentCheck(params.primary);
            if (!success) {
                valImpl.addValueSilent(val, std::move(removedMember));
            }
            if (!success && ++numberTries < params.parentCheckTryLimit) {
                break;
            }
        } while (!success);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find value, number tries=" << numberTries);
            return;
        }
        debug_neighbourhood_action("Removed " << *removedMember);
        val.signalValueRemoved(getValueHash(*removedMember));
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            valImpl.addValue(val, std::move(removedMember));
        }
    });
}

void setRemoveGen(const SetDomain& domain,
                  std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.sizeAttr.sizeType == SizeAttr::SizeAttrType::EXACT_SIZE) {
        return;
    }
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setRemoveGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

template <typename InnerDomainPtrType>
void setLiftSingleGenImpl(const SetDomain& domain,
                          const InnerDomainPtrType& innerDomainPtr,
                          std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(domain.inner, innerDomainNeighbourhoods);
    u_int64_t innerDomainSize = getDomainSize(domain.inner);
    typedef ValRef<typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type>
        InnerValRefType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back("setLiftSingle_" + innerNh.name, [
            innerNhApply{std::move(innerNh.apply)}, innerDomainSize, &domain,
            &innerDomainPtr
        ](NeighbourhoodParams & params) {
            auto& val = *mpark::get<ValRef<SetValue>>(params.primary);
            auto& valImpl =
                mpark::get<SetValueImpl<InnerValRefType>>(val.setValueImpl);
            if (valImpl.members.size() == 0) {
                ++params.stats.minorNodeCount;
                return;
            }
            ViolationDescription& vioDescAtThisLevel =
                params.vioDesc.hasChildViolation(val.id)
                    ? params.vioDesc.childViolations(val.id)
                    : emptyViolations;
            u_int64_t indexToChange =
                (vioDescAtThisLevel.getTotalViolation() != 0)
                    ? vioDescAtThisLevel.selectRandomVar()
                    : globalRandom<u_int64_t>(0, valImpl.members.size() - 1);
            valImpl.possibleMemberValueChange(val, indexToChange);
            ParentCheckCallBack parentCheck = [&](const AnyValRef& newValue) {
                u_int64_t newHash = getValueHash(newValue);
                if (val.getMemberHashes().count(newHash)) {
                    return false;
                }
                val.removeHash(valImpl.hashOfPossibleChange);
                val.addHash(newHash);
                bool allowedByParent = params.parentCheck(params.primary);
                val.removeHash(newHash);
                val.addHash(valImpl.hashOfPossibleChange);
                return allowedByParent;
            };
            bool requiresRevert = false;

            AnyValRef changingMember = valImpl.members[indexToChange];
            AcceptanceCallBack changeAccepted = [&]() {
                valImpl.memberValueChanged(val, indexToChange);
                requiresRevert = !params.changeAccepted();
                if (requiresRevert) {
                    valImpl.possibleMemberValueChange(val, indexToChange);
                }
                return !requiresRevert;
            };
            NeighbourhoodParams innerNhParams(
                changeAccepted, parentCheck,
                getTryLimit(valImpl.members.size(), innerDomainSize),
                changingMember, params.stats, vioDescAtThisLevel);
            innerNhApply(innerNhParams);
            if (requiresRevert) {
                valImpl.memberValueChanged(val, indexToChange);
            }
        });
    }
}

void setLiftSingleGen(const SetDomain& domain,
                      std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            setLiftSingleGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

const NeighbourhoodGenList<SetDomain>::type
    NeighbourhoodGenList<SetDomain>::value = {setLiftSingleGen, setAddGen,
                                              setRemoveGen};
