#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/function.h"
#include "types/int.h"
#include "utils/random.h"

using namespace std;

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
                               const InnerDomainPtrType&,
                               int numberValsRequired,
                               std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.to, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "functionLiftSingle_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<FunctionValue>().front());
                if (val.rangeSize() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id);
                UInt indexToChange = vioContainerAtThisLevel.selectRandomVar(
                    val.rangeSize() - 1);
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
                    params.stats, vioContainerAtThisLevel);
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
void functionLiftMultipleGenImpl(const FunctionDomain& domain,
                                 const InnerDomainPtrType&,
                                 int numberValsRequired,
                                 std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(0, domain.to, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        if (innerNh.numberValsRequired < 2) {
            continue;
        }

        neighbourhoods.emplace_back(
            "functionLiftMultiple_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerNhNumberValsRequired{innerNh.numberValsRequired}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<FunctionValue>().front());
                if (val.rangeSize() < (size_t)innerNhNumberValsRequired) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id);
                std::vector<UInt> indicesToChange =
                    vioContainerAtThisLevel.selectRandomVars(
                        val.rangeSize() - 1, innerNhNumberValsRequired);
                debug_log(indicesToChange);
                val.notifyPossibleImagesChange(indicesToChange);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {

                    return val.tryImagesChange<InnerValueType>(
                        indicesToChange,
                        [&]() { return params.parentCheck(params.vals); });
                };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        val.notifyPossibleImagesChange(indicesToChange);
                    }
                    return !requiresRevert;
                };
                AnyValVec changingMembers;
                auto& changingMembersImpl =
                    changingMembers.emplace<ValRefVec<InnerValueType>>();
                for (UInt indexToChange : indicesToChange) {
                    changingMembersImpl.emplace_back(
                        val.member<InnerValueType>(indexToChange));
                }
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, params.parentCheckTryLimit,
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryImagesChange<InnerValueType>(indicesToChange,
                                                        [&]() { return true; });
                }
            });
    }
}

void functionLiftMultipleGen(const FunctionDomain& domain,
                             int numberValsRequired,
                             std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            functionLiftMultipleGenImpl(domain, innerDomainPtr,
                                        numberValsRequired, neighbourhoods);
        },
        domain.to);
}

template <typename InnerDomainPtrType>
void functionImagesSwapGenImpl(const FunctionDomain&, InnerDomainPtrType&,
                               int numberValsRequired,
                               std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;

    neighbourhoods.emplace_back(
        "functionImagesSwap", numberValsRequired,
        [](NeighbourhoodParams& params) {
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
        overloaded(
            [&](const shared_ptr<BoolDomain>& innerDomainPtr) {
                functionImagesSwapGenImpl(domain, innerDomainPtr,
                                          numberValsRequired, neighbourhoods);
            },
            [&](const shared_ptr<IntDomain>& innerDomainPtr) {
                functionImagesSwapGenImpl(domain, innerDomainPtr,
                                          numberValsRequired, neighbourhoods);
            },
            [&](const auto& innerDomainPtr) {
                if (false) {
                    functionImagesSwapGenImpl(domain, innerDomainPtr,
                                              numberValsRequired,
                                              neighbourhoods);
                }
            }),
        domain.to);
}
void functionUnifyImagesGenImpl(const FunctionDomain&,
                                const shared_ptr<IntDomain>& innerDomainPtr,
                                int numberValsRequired,
                                std::vector<Neighbourhood>& neighbourhoods) {
    UInt innerDomainSize = innerDomainPtr->domainSize;
    neighbourhoods.emplace_back(
        "functionUnifyImages", numberValsRequired,
        [innerDomainSize](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<FunctionValue>().front());
            if (val.rangeSize() < 2) {
                ++params.stats.minorNodeCount;
                return;
            }
            int numberTries = 0;
            const int tryLimit =
                params.parentCheckTryLimit *
                calcNumberInsertionAttempts(val.rangeSize(), innerDomainSize);
            debug_neighbourhood_action("Looking for indices to unitfy");
            bool success;
            UInt index1, index2;
            Int valueBackup;

            do {
                ++params.stats.minorNodeCount;
                ++params.stats.minorNodeCount;
                index1 = globalRandom<UInt>(0, val.rangeSize() - 2);
                index2 = globalRandom<UInt>(index1 + 1, val.rangeSize() - 1);
                if (val.getRange<IntView>()[index1]->view()->value ==
                    val.getRange<IntView>()[index2]->view()->value) {
                    continue;
                }
                // randomly swap indices
                if (globalRandom(0, 1)) {
                    swap(index1, index2);
                }
                auto& view1 = val.getRange<IntView>()[index1]->view().get();
                auto& view2 = val.getRange<IntView>()[index2]->view().get();
                val.notifyPossibleImageChange(index2);
                valueBackup = view2.value;
                success = val.tryImageChange<IntValue>(index2, [&]() {
                    return view2.changeValue([&]() {
                        view2.value = view1.value;
                        if (params.parentCheck(params.vals)) {
                            return true;
                        } else {
                            view2.value = valueBackup;
                            return false;
                        }
                    });
                });
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find images to unify, number tries=" << tryLimit);
                return;
            }
            debug_neighbourhood_action("images unified: " << index1 << " and "
                                                          << index2);
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                val.notifyPossibleImageChange(index2);
                auto& view2 = val.getRange<IntView>()[index2]->view().get();
                val.tryImageChange<IntValue>(index2, [&]() {
                    return view2.changeValue([&]() {
                        view2.value = valueBackup;
                        return true;
                    });
                });
            }
        });
}

void functionUnifyImagesGen(const FunctionDomain& domain,
                            int numberValsRequired,
                            std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(overloaded(
                     [&](const shared_ptr<IntDomain>& innerDomainPtr) {
                         functionUnifyImagesGenImpl(domain, innerDomainPtr,
                                                    numberValsRequired,
                                                    neighbourhoods);
                     },
                     [&](const auto&) {}),
                 domain.to);
}

void functionSplitImagesGenImpl(const FunctionDomain&,
                                const shared_ptr<IntDomain>& innerDomainPtr,
                                int numberValsRequired,
                                std::vector<Neighbourhood>& neighbourhoods) {
    UInt innerDomainSize = innerDomainPtr->domainSize;
    neighbourhoods.emplace_back(
        "functionSplitImages", numberValsRequired,
        [innerDomainSize, &innerDomainPtr](NeighbourhoodParams& params) {
            auto& val = *(params.getVals<FunctionValue>().front());
            if (val.rangeSize() < 2) {
                ++params.stats.minorNodeCount;
                return;
            }
            int numberTries = 0;
            const int tryLimit =
                params.parentCheckTryLimit *
                calcNumberInsertionAttempts(val.rangeSize(), innerDomainSize);
            debug_neighbourhood_action("Looking for indices to split");
            bool success;
            UInt index1, index2;
            Int valueBackup;

            do {
                ++params.stats.minorNodeCount;
                index1 = globalRandom<UInt>(0, val.rangeSize() - 2);
                index2 = globalRandom<UInt>(index1 + 1, val.rangeSize() - 1);
                if (val.getRange<IntView>()[index1]->view()->value !=
                    val.getRange<IntView>()[index2]->view()->value) {
                    continue;
                }
                // randomly swap indices
                if (globalRandom(0, 1)) {
                    swap(index1, index2);
                }
                auto& view1 = val.getRange<IntView>()[index1]->view().get();
                static_cast<void>(view1);
                auto value2 = val.member<IntValue>(index2);
                val.notifyPossibleImageChange(index2);
                valueBackup = value2->value;
                success = val.tryImageChange<IntValue>(index2, [&]() {
                    return value2->changeValue([&]() {
                        assignRandomValueInDomain(*innerDomainPtr, *value2,
                                                  params.stats);
                        if (value2->value != valueBackup &&
                            params.parentCheck(params.vals)) {
                            return true;
                        } else {
                            value2->value = valueBackup;
                            return false;
                        }
                    });
                });
            } while (!success && ++numberTries < tryLimit);
            if (!success) {
                debug_neighbourhood_action(
                    "Couldn't find images to split, number tries=" << tryLimit);
                return;
            }
            debug_neighbourhood_action("images split: " << index1 << " and "
                                                        << index2);
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                val.notifyPossibleImageChange(index2);
                auto& view2 = val.getRange<IntView>()[index2]->view().get();
                val.tryImageChange<IntValue>(index2, [&]() {
                    return view2.changeValue([&]() {
                        view2.value = valueBackup;
                        return true;
                    });
                });
            }
        });
}

void functionSplitImagesGen(const FunctionDomain& domain,
                            int numberValsRequired,
                            std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(overloaded(
                     [&](const shared_ptr<IntDomain>& innerDomainPtr) {
                         functionSplitImagesGenImpl(domain, innerDomainPtr,
                                                    numberValsRequired,
                                                    neighbourhoods);
                     },
                     [&](const auto&) {}),
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
        {1, functionLiftSingleGen},    //
        {1, functionLiftMultipleGen},  //
        {1, functionImagesSwapGen},    //
        {1, functionUnifyImagesGen},   //
        {1, functionSplitImagesGen},
};
