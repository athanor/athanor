#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/tupleVal.h"
#include "utils/random.h"

using namespace std;

template <>
bool assignRandomValueInDomain<TupleDomain>(
    const TupleDomain& domain, TupleValue& val,
    NeighbourhoodResourceTracker& resource) {
    // populate tuple with unassigned members if it is empty
    val.silentClear();
    while (val.members.size() < domain.inners.size()) {
        lib::visit(
            [&](auto& innerDomainPtr) {
                auto newMember = constructValueFromDomain(*innerDomainPtr);
                val.addMember(newMember);
            },
            domain.inners[val.members.size()]);
    }
    for (size_t i = 0; i < domain.inners.size(); i++) {
        auto reserved =
            resource.reserve(domain.inners.size(), domain.inners[i], i);

        bool success = lib::visit(
            [&](const auto& innerDomainPtr) {
                typedef
                    typename BaseType<decltype(innerDomainPtr)>::element_type
                        Domain;
                typedef typename AssociatedValueType<Domain>::type Value;
                auto& memberVal = *val.member<Value>(i);
                return assignRandomValueInDomain(*innerDomainPtr, memberVal,
                                                 resource);
            },
            domain.inners[i]);
        if (!success) {
            return false;
        }
    }
    return true;
}

void tupleAssignRandomGen(const TupleDomain& domain, int numberValsRequired,
                          std::vector<Neighbourhood>& neighbourhoods) {
    for (auto& inner : domain.inners) {
        if (!lib::get_if<shared_ptr<IntDomain>>(&inner) &&
            !lib::get_if<shared_ptr<BoolDomain>>(&inner) &&
            !lib::get_if<shared_ptr<EnumDomain>>(&inner)) {
            return;
        }
    }
    neighbourhoods.emplace_back(
        "TupleAssignRandom", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<TupleValue>().front());
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            debug_neighbourhood_action(
                "Assigning random value: original value is " << asView(val));
            auto backup = deepCopy(val);
            backup->container = val.container;
            auto newValue = constructValueFromDomain(domain);
            newValue->container = val.container;
            bool success;
            NeighbourhoodResourceAllocator allocator(domain);
            do {
                auto resource = allocator.requestLargerResource();
                success =
                    assignRandomValueInDomain(domain, *newValue, resource);
                params.stats.minorNodeCount += resource.getResourceConsumed();
                success = success && val.tryAssignNewValue(*newValue, [&]() {
                    return params.parentCheck(params.vals);
                });
                if (success) {
                    debug_neighbourhood_action("New value is " << asView(val));
                }
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find value, number tries=" << tryLimit);
                return;
            }
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                deepCopy(*backup, val);
            }
        });
}

template <typename InnerDomainPtrType>
void tupleLiftSingleGenImpl(const TupleDomain& domain,
                            const InnerDomainPtrType&, int numberValsRequired,
                            size_t indexToChange,
                            std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.inners[indexToChange],
                           innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            toString("TupleLiftSingle_index", indexToChange, "_", innerNh.name),
            numberValsRequired,
            [indexToChange, innerNhApply{std::move(innerNh.apply)}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<TupleValue>().front());
                debug_code(assert(indexToChange < val.members.size()));
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val.tryMemberChange<InnerValueType>(
                        indexToChange,
                        [&]() { return params.parentCheck(params.vals); });
                };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    return !requiresRevert;
                };
                AnyValVec changingMembers;
                auto& changingMembersImpl =
                    changingMembers.emplace<ValRefVec<InnerValueType>>();
                changingMembersImpl.emplace_back(
                    val.member<InnerValueType>(indexToChange));
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, params.parentCheckTryLimit,
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryMemberChange<InnerValueType>(indexToChange,
                                                        [&]() { return true; });
                }
            });
    }
}

void tupleLiftSingleGen(const TupleDomain& domain, int numberValsRequired,
                        std::vector<Neighbourhood>& neighbourhoods) {
    for (size_t i = 0; i < domain.inners.size(); i++) {
        lib::visit(
            [&](const auto& innerDomainPtr) {
                tupleLiftSingleGenImpl(domain, innerDomainPtr,
                                       numberValsRequired, i, neighbourhoods);
            },
            domain.inners[i]);
    }
}

template <typename InnerDomainPtrType>
void tupleLiftSingleTwoVarsGenImpl(const TupleDomain& domain,
                                   const InnerDomainPtrType&,
                                   int numberValsRequired, size_t indexToChange,
                                   std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(2, domain.inners[indexToChange],
                           innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        if (innerNh.numberValsRequired != 2) {
            continue;
        }
        neighbourhoods.emplace_back(
            toString("tupleLiftSingleTwoVars_index", indexToChange, "_",
                     innerNh.name),
            numberValsRequired,
            [indexToChange, innerNhApply{std::move(innerNh.apply)}](
                NeighbourhoodParams& params) {
                auto& val1 = *(params.getVals<TupleValue>()[0]);
                auto& val2 = *(params.getVals<TupleValue>()[1]);
                debug_code(assert(indexToChange < val1.members.size()));
                auto& vioContainerAtThisLevel = emptyViolations;
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val1.tryMemberChange<InnerValueType>(
                        indexToChange, [&]() {
                            return val2.tryMemberChange<InnerValueType>(
                                indexToChange, [&]() {
                                    return params.parentCheck(params.vals);
                                });
                        });
                };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    return !requiresRevert;
                };
                AnyValVec changingMembers;
                auto& changingMembersImpl =
                    changingMembers.emplace<ValRefVec<InnerValueType>>();
                changingMembersImpl.emplace_back(
                    val1.member<InnerValueType>(indexToChange));
                changingMembersImpl.emplace_back(
                    val2.member<InnerValueType>(indexToChange));
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, params.parentCheckTryLimit,
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val1.tryMemberChange<InnerValueType>(
                        indexToChange, [&]() { return true; });
                    val2.tryMemberChange<InnerValueType>(
                        indexToChange, [&]() { return true; });
                }
            });
    }
}

void tupleLiftSingleTwoVarsGen(const TupleDomain& domain,
                               int numberValsRequired,
                               std::vector<Neighbourhood>& neighbourhoods) {
    for (size_t i = 0; i < domain.inners.size(); i++) {
        lib::visit(
            [&](const auto& innerDomainPtr) {
                tupleLiftSingleTwoVarsGenImpl(domain, innerDomainPtr,
                                              numberValsRequired, i,
                                              neighbourhoods);
            },
            domain.inners[i]);
    }
}

const NeighbourhoodVec<TupleDomain> NeighbourhoodGenList<TupleDomain>::value = {
    {1, tupleAssignRandomGen},      //
    {1, tupleLiftSingleGen},        //
    {2, tupleLiftSingleTwoVarsGen}  //
};

const NeighbourhoodVec<TupleDomain>
    NeighbourhoodGenList<TupleDomain>::mergeNeighbourhoods = {};
const NeighbourhoodVec<TupleDomain>
    NeighbourhoodGenList<TupleDomain>::splitNeighbourhoods = {};
