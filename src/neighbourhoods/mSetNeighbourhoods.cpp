#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "types/mSet.h"
#include "utils/random.h"

using namespace std;
static ViolationDescription emptyViolations;

template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const MSetDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   MSetValue& val) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    size_t newNumberElements =
        globalRandom(domain.sizeAttr.minSize, domain.sizeAttr.maxSize);
    // clear mSet and populate with new random elements
    while (val.numberElements() > 0) {
        val.removeMember<InnerValueType>(val.numberElements() - 1);
    }
    while (newNumberElements > val.numberElements()) {
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        assignRandomValueInDomain(*innerDomainPtr, *newMember);
        val.addMember(newMember);
    }
}

template <>
void assignRandomValueInDomain<MSetDomain>(const MSetDomain& domain,
                                           MSetValue& val) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val);
        },
        domain.inner);
}

inline int getTryLimit(UInt, UInt) { return 1; }

template <typename InnerDomainPtrType>
void mSetLiftSingleGenImpl(const MSetDomain& domain,
                           const InnerDomainPtrType& innerDomainPtr,
                           std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(domain.inner, innerDomainNeighbourhoods);
    UInt innerDomainSize = getDomainSize(domain.inner);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "mSetLiftSingle_" + innerNh.name,
            [innerNhApply{std::move(innerNh.apply)}, innerDomainSize, &domain,
             &innerDomainPtr](NeighbourhoodParams& params) {
                auto& val = *mpark::get<ValRef<MSetValue>>(params.primary);
                if (val.numberElements() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                ViolationDescription& vioDescAtThisLevel =
                    params.vioDesc.hasChildViolation(val.id)
                        ? params.vioDesc.childViolations(val.id)
                        : emptyViolations;
                UInt indexToChange =
                    (vioDescAtThisLevel.getTotalViolation() != 0)
                        ? vioDescAtThisLevel.selectRandomVar()
                        : globalRandom<UInt>(0, val.numberElements() - 1);
                val.notifyPossibleMemberChange<InnerValueType>(indexToChange);
                ParentCheckCallBack parentCheck = [&](const AnyValRef&) {
                    return val.tryMemberChange<InnerValueType>(
                        indexToChange,
                        [&]() { return params.parentCheck(params.primary); });
                };
                bool requiresRevert = false;
                AnyValRef changingMember =
                    val.member<InnerValueType>(indexToChange);
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        val.notifyPossibleMemberChange<InnerValueType>(
                            indexToChange);
                    }
                    return !requiresRevert;
                };
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck,
                    getTryLimit(val.numberElements(), innerDomainSize),
                    changingMember, params.stats, vioDescAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryMemberChange<InnerValueType>(indexToChange,
                                                        [&]() { return true; });
                }
            });
    }
}

void mSetLiftSingleGen(const MSetDomain& domain,
                       std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            mSetLiftSingleGenImpl(domain, innerDomainPtr, neighbourhoods);
        },
        domain.inner);
}

const NeighbourhoodVec<MSetDomain> NeighbourhoodGenList<MSetDomain>::value = {
    mSetLiftSingleGen};
