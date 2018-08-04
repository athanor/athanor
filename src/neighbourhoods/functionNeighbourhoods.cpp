#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/function.h"
#include "utils/random.h"

using namespace std;
static ViolationContainer emptyViolations;

template <typename InnerDomainPtrType>
void assignRandomValueInDomainImpl(const FunctionDomain& domain,
                                   const InnerDomainPtrType& innerDomainPtr,
                                   FunctionValue& val, StatsContainer& stats) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    val.resetDimensions<InnerViewType>(
        FunctionView::makeDimensionVecFromDomain(domain.from));
    for (size_t i = 0; i < val.rangeSize(); i++) {
        auto newMember = constructValueFromDomain(*innerDomainPtr);
        assignRandomValueInDomain(*innerDomainPtr, *newMember, stats);
        val.assignImage<InnerValueType>(i, newMember);
    }
}

template <>
void assignRandomValueInDomain<FunctionDomain>(const FunctionDomain& domain,
                                               FunctionValue& val,
                                               StatsContainer& stats) {
    mpark::visit(
        [&](auto& innerDomainPtr) {
            assignRandomValueInDomainImpl(domain, innerDomainPtr, val, stats);
        },
        domain.to);
}

template <typename InnerDomainPtrType>
void functionLiftSingleGenImpl(const FunctionDomain& domain,
                               const InnerDomainPtrType& innerDomainPtr,
                               int numberValsRequired,
                               std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.to, innerDomainNeighbourhoods);
    UInt innerDomainSize = getDomainSize(domain.to);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "functionLiftSingle_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)}, innerDomainSize, &domain,
             &innerDomainPtr](NeighbourhoodParams& params) {
                auto& val = *(params.getVals<FunctionValue>().front());
                if (val.rangeSize() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                ViolationContainer& vioDescAtThisLevel =
                    params.vioDesc.hasChildViolation(val.id)
                        ? params.vioDesc.childViolations(val.id)
                        : emptyViolations;
                UInt indexToChange =
                    vioDescAtThisLevel.selectRandomVar(val.rangeSize() - 1);
                val.notifyPossibleImageChange(indexToChange);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val.tryImageChange<InnerValueType>(
                        indexToChange,
                        [&]() { return params.parentCheck(params.vals); });
                };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        val.notifyPossibleImageChange(indexToChange);
                    }
                    return !requiresRevert;
                };
                AnyValVec changingMembers;
                auto& changingMembersImpl =
                    changingMembers.emplace<ValRefVec<InnerValueType>>();
                changingMembersImpl.emplace_back(
                    val.member<InnerValueType>(indexToChange));

                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, 1, changingMembers,
                    params.stats, vioDescAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryImageChange<InnerValueType>(indexToChange,
                                                       [&]() { return true; });
                }
            });
    }
}

void functionLiftSingleGen(const FunctionDomain& domain, int numberValsRequired,
                           std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            functionLiftSingleGenImpl(domain, innerDomainPtr,
                                      numberValsRequired, neighbourhoods);
        },
        domain.to);
}

template <typename InnerDomainPtrType>
void functionImagesSwapGenImpl(const FunctionDomain& domain,
                               InnerDomainPtrType& innerDomainPtr,
                               int numberValsRequired,
                               std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;

    neighbourhoods.emplace_back(
        "functionImagesSwap", numberValsRequired,
        [&domain, &innerDomainPtr](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<FunctionValue>().front());
            if (val.rangeSize() < 2) {
                ++params.stats.minorNodeCount;
                return;
            }
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            debug_neighbourhood_action("Looking for indices to swap");
            bool success;
            UInt index1, index2;
            do {
                ++params.stats.minorNodeCount;
                index1 = globalRandom<UInt>(0, val.rangeSize() - 2);
                index2 = globalRandom<UInt>(index1 + 1, val.rangeSize() - 1);
                success = val.trySwapImages<InnerValueType>(
                    index1, index2,
                    [&]() { return params.parentCheck(params.vals); });
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find positions to swap, number tries="
                    << tryLimit);
                return;
            }
            debug_neighbourhood_action(
                "positions swapped: " << index1 << " and " << index2);
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                val.trySwapImages<InnerValueType>(index1, index2,
                                                  []() { return true; });
            }
        });
}

void functionImagesSwapGen(const FunctionDomain& domain, int numberValsRequired,
                           std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            functionImagesSwapGenImpl(domain, innerDomainPtr,
                                      numberValsRequired, neighbourhoods);
        },
        domain.to);
}

void functionAssignRandomGen(const FunctionDomain& domain,
                             int numberValsRequired,
                             std::vector<Neighbourhood>& neighbourhoods) {
    neighbourhoods.emplace_back(
        "functionAssignRandom", numberValsRequired,
        [&domain](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<FunctionValue>().front());
            int numberTries = 0;
            const int tryLimit = params.parentCheckTryLimit;
            debug_neighbourhood_action(
                "Assigning random value: original value is " << asView(val));
            auto backup = deepCopy(val);
            backup->container = val.container;
            auto newValue = constructValueFromDomain(domain);
            newValue->container = val.container;
            bool success;
            do {
                assignRandomValueInDomain(domain, *newValue, params.stats);
                success = val.tryAssignNewValue(*newValue, [&]() {
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

const NeighbourhoodVec<FunctionDomain>
    NeighbourhoodGenList<FunctionDomain>::value = {
        {1, functionLiftSingleGen},  //
        {1, functionImagesSwapGen}};
