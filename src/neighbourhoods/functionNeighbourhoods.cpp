#include <cmath>
#include <random>

#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/boolVal.h"
#include "types/functionVal.h"
#include "types/intVal.h"
#include "types/tupleVal.h"
#include "utils/random.h"

using namespace std;

template <typename PreimageDomainPtrType, typename ImageDomainPtrType>
bool assignRandomFunctionUsingExplicitPreimages(
    const FunctionDomain& domain,
    const PreimageDomainPtrType& preimageDomainPtr,
    const ImageDomainPtrType& imageDomainPtr, FunctionValue& val,
    NeighbourhoodResourceTracker& resource) {
    typedef typename AssociatedValueType<
        typename PreimageDomainPtrType::element_type>::type PreimageValueType;
    typedef typename AssociatedValueType<
        typename ImageDomainPtrType::element_type>::type ImageValueType;
    typedef typename AssociatedViewType<ImageValueType>::type ImageViewType;
    typedef
        typename AssociatedViewType<PreimageValueType>::type PreimageViewType;
    ExplicitPreimageContainer preimageContainer;
    preimageContainer.preimages.emplace<ExprRefVec<PreimageViewType>>();
    ExprRefVec<ImageViewType> range;
    auto newNumberElementsOption = resource.randomNumberElements(
        domain.sizeAttr.minSize, domain.sizeAttr.maxSize, preimageDomainPtr);
    if (!newNumberElementsOption) {
        return false;
    }
    size_t newNumberElements = *newNumberElementsOption;
    while (newNumberElements > range.size()) {
        auto reserved = resource.reserve(domain.sizeAttr.minSize,
                                         preimageDomainPtr, range.size());
        auto reserved2 = resource.reserve(domain.sizeAttr.minSize,
                                          imageDomainPtr, range.size());
        auto preimage = constructValueFromDomain(*preimageDomainPtr);
        bool success = false;
        do {
            success = assignRandomValueInDomain(*preimageDomainPtr, *preimage,
                                                resource);
            if (!success) {
                break;
                ;
            }
        } while (!preimageContainer.add(preimage.asExpr()));
        if (!success) {
            break;
        }
        auto image = constructValueFromDomain(*imageDomainPtr);
        success = assignRandomValueInDomain(*imageDomainPtr, *image, resource);
        if (!success) {
            preimageContainer.remove<PreimageViewType>(range.size());
            break;
        }
        range.emplace_back(image.asExpr());
    }
    if (range.size() >= domain.sizeAttr.minSize) {
        val.initVal(domain.from, std::move(preimageContainer), std::move(range),
                    domain.partial == PartialAttr::PARTIAL);

        return true;
    }
    return false;
}

template <typename PreimageDomainPtrType, typename ImageDomainPtrType>
bool assignRandomFunctionUsingDimensions(
    const FunctionDomain& domain, const PreimageDomainPtrType&,
    const ImageDomainPtrType& imageDomainPtr, FunctionValue& val,
    NeighbourhoodResourceTracker& resource) {
    typedef typename AssociatedValueType<
        typename ImageDomainPtrType::element_type>::type ImageValueType;
    typedef typename AssociatedViewType<ImageValueType>::type ImageViewType;
    ExprRefVec<ImageViewType> range;
    while (range.size() < domain.sizeAttr.minSize) {
        auto reserved = resource.reserve(domain.sizeAttr.minSize,
                                         imageDomainPtr, range.size());
        auto newMember = constructValueFromDomain(*imageDomainPtr);
        bool success =
            assignRandomValueInDomain(*imageDomainPtr, *newMember, resource);
        if (!success) {
            return false;
        }
        range.emplace_back(newMember.asExpr());
    }
    val.initVal(domain.from, makeDimensionVecFromDomain(domain.from),
                std::move(range), false);
    return true;
}

template <typename PreimageDomainPtrType, typename ImageDomainPtrType>
bool assignRandomValueInDomainImpl(
    const FunctionDomain& domain,
    const PreimageDomainPtrType& preimageDomainPtr,
    const ImageDomainPtrType& imageDomainPtr, FunctionValue& val,
    NeighbourhoodResourceTracker& resource) {
    bool useDimensions = domain.partial == PartialAttr::TOTAL &&
                         canBuildDimensionVec(domain.from);

    if (useDimensions) {
        return assignRandomFunctionUsingDimensions(
            domain, preimageDomainPtr, imageDomainPtr, val, resource);
    } else {
        return assignRandomFunctionUsingExplicitPreimages(
            domain, preimageDomainPtr, imageDomainPtr, val, resource);
    }
}

template <>
bool assignRandomValueInDomain<FunctionDomain>(
    const FunctionDomain& domain, FunctionValue& val,
    NeighbourhoodResourceTracker& resource) {
    return lib::visit(
        [&](auto& preimageDomainPtr, auto& imageDomainPtr) {
            return assignRandomValueInDomainImpl(domain, preimageDomainPtr,
                                                 imageDomainPtr, val, resource);
        },
        domain.from, domain.to);
}

template <typename InnerDomainPtrType>
void functionLiftImageSingleGenImpl(
    const FunctionDomain& domain, const InnerDomainPtrType&,
    int numberValsRequired, std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.to, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "functionLiftImageSingle_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<FunctionValue>().front());
                if (val.rangeSize() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id).childViolations(
                        FunctionValue::IMAGE_VALBASE_INDEX);
                UInt indexToChange = vioContainerAtThisLevel.selectRandomVar(
                    val.rangeSize() - 1);

                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val.tryImageChange<InnerValueType>(
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
                    val.getImageVal<InnerValueType>(indexToChange));

                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, params.parentCheckTryLimit,
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryImageChange<InnerValueType>(indexToChange,

                                                       [&]() { return true; });
                }
            });
    }
}

void functionLiftImageSingleGen(const FunctionDomain& domain,
                                int numberValsRequired,
                                std::vector<Neighbourhood>& neighbourhoods) {
    lib::visit(
        [&](const auto& innerDomainPtr) {
            functionLiftImageSingleGenImpl(domain, innerDomainPtr,
                                           numberValsRequired, neighbourhoods);
        },
        domain.to);
}

template <typename InnerDomainPtrType>
void functionLiftPreimageSingleGenImpl(
    const FunctionDomain& domain, const InnerDomainPtrType&,
    int numberValsRequired, std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(1, domain.from, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        neighbourhoods.emplace_back(
            "functionLiftPreimageSingle_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<FunctionValue>().front());
                if (val.rangeSize() == 0) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id).childViolations(
                        FunctionValue::PREIMAGE_VALBASE_INDEX);
                UInt indexToChange = vioContainerAtThisLevel.selectRandomVar(
                    val.rangeSize() - 1);

                ParentCheckCallBack parentCheck = [&](const AnyValVec&
                                                          newValue) {
                    HashType newHash = getValueHash(
                        lib::get<ValRefVec<InnerValueType>>(newValue).front());
                    if (val.getPreimages().preimageHashIndexMap.count(
                            newHash)) {
                        return false;
                    }
                    return val.tryPreimageChange<InnerValueType>(
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
                    val.getPreimageVal<InnerValueType>(indexToChange));

                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, params.parentCheckTryLimit,
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryPreimageChange<InnerValueType>(
                        indexToChange,

                        [&]() { return true; });
                }
            });
    }
}

void functionLiftPreimageSingleGen(const FunctionDomain& domain,
                                   int numberValsRequired,
                                   std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.partial == PartialAttr::TOTAL) {
        return;
    }
    lib::visit(
        [&](const auto& innerDomainPtr) {
            functionLiftPreimageSingleGenImpl(
                domain, innerDomainPtr, numberValsRequired, neighbourhoods);
        },
        domain.from);
}

template <typename InnerValueType>
bool functionPreimageDoesNotContain(
    FunctionValue& val, const ValRefVec<InnerValueType>& newMembers) {
    size_t lastInsertedIndex = 0;
    auto& preimages = val.getPreimages();
    do {
        HashType hash = getValueHash(newMembers[lastInsertedIndex]);
        bool inserted = preimages.preimageHashIndexMap.emplace(hash, 0).second;
        if (!inserted) {
            break;
        }
    } while (++lastInsertedIndex < newMembers.size());
    for (size_t i = 0; i < lastInsertedIndex; i++) {
        preimages.preimageHashIndexMap.erase(getValueHash(newMembers[i]));
    }
    return lastInsertedIndex == newMembers.size();
}

template <typename InnerDomainPtrType>
void functionLiftPreimageMultipleGenImpl(
    const FunctionDomain& domain, const InnerDomainPtrType&,
    int numberValsRequired, std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(0, domain.from, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        if (innerNh.numberValsRequired < 2) {
            continue;
        }

        neighbourhoods.emplace_back(
            "functionLiftPreimageMultiple_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerNhNumberValsRequired{innerNh.numberValsRequired}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<FunctionValue>().front());
                if (val.rangeSize() < (size_t)innerNhNumberValsRequired) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id).childViolations(
                        FunctionValue::PREIMAGE_VALBASE_INDEX);
                std::vector<UInt> indicesToChange =
                    vioContainerAtThisLevel.selectRandomVars(
                        val.rangeSize() - 1, innerNhNumberValsRequired);
                debug_log(indicesToChange);
                ParentCheckCallBack parentCheck =
                    [&](const AnyValVec& newMembers) {
                        auto& newMembersImpl =
                            lib::get<ValRefVec<InnerValueType>>(newMembers);
                        return functionPreimageDoesNotContain(val,
                                                              newMembersImpl) &&
                               val.tryPreimagesChange<InnerValueType>(
                                   indicesToChange, [&]() {
                                       return params.parentCheck(params.vals);
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
                for (UInt indexToChange : indicesToChange) {
                    changingMembersImpl.emplace_back(
                        val.getPreimageVal<InnerValueType>(indexToChange));
                }
                NeighbourhoodParams innerNhParams(
                    changeAccepted, parentCheck, params.parentCheckTryLimit,
                    changingMembers, params.stats, vioContainerAtThisLevel);
                innerNhApply(innerNhParams);
                if (requiresRevert) {
                    val.tryPreimagesChange<InnerValueType>(
                        indicesToChange, [&]() { return true; });
                }
            });
    }
}

void functionLiftPreimageMultipleGen(
    const FunctionDomain& domain, int numberValsRequired,
    std::vector<Neighbourhood>& neighbourhoods) {
    if (domain.partial == PartialAttr::TOTAL) {
        return;
    }
    lib::visit(
        [&](const auto& innerDomainPtr) {
            functionLiftPreimageMultipleGenImpl(
                domain, innerDomainPtr, numberValsRequired, neighbourhoods);
        },
        domain.from);
}

template <typename InnerDomainPtrType>
void functionLiftImageMultipleGenImpl(
    const FunctionDomain& domain, const InnerDomainPtrType&,
    int numberValsRequired, std::vector<Neighbourhood>& neighbourhoods) {
    std::vector<Neighbourhood> innerDomainNeighbourhoods;
    generateNeighbourhoods(0, domain.to, innerDomainNeighbourhoods);
    typedef typename AssociatedValueType<
        typename InnerDomainPtrType::element_type>::type InnerValueType;
    for (auto& innerNh : innerDomainNeighbourhoods) {
        if (innerNh.numberValsRequired < 2) {
            continue;
        }

        neighbourhoods.emplace_back(
            "functionLiftImageMultiple_" + innerNh.name, numberValsRequired,
            [innerNhApply{std::move(innerNh.apply)},
             innerNhNumberValsRequired{innerNh.numberValsRequired}](
                NeighbourhoodParams& params) {
                auto& val = *(params.getVals<FunctionValue>().front());
                if (val.rangeSize() < (size_t)innerNhNumberValsRequired) {
                    ++params.stats.minorNodeCount;
                    return;
                }
                auto& vioContainerAtThisLevel =
                    params.vioContainer.childViolations(val.id).childViolations(
                        FunctionValue::IMAGE_VALBASE_INDEX);
                std::vector<UInt> indicesToChange =
                    vioContainerAtThisLevel.selectRandomVars(
                        val.rangeSize() - 1, innerNhNumberValsRequired);
                debug_log(indicesToChange);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val.tryImagesChange<InnerValueType>(
                        indicesToChange,
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
                for (UInt indexToChange : indicesToChange) {
                    changingMembersImpl.emplace_back(
                        val.getImageVal<InnerValueType>(indexToChange));
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

void functionLiftImageMultipleGen(const FunctionDomain& domain,
                                  int numberValsRequired,
                                  std::vector<Neighbourhood>& neighbourhoods) {
    lib::visit(
        [&](const auto& innerDomainPtr) {
            functionLiftImageMultipleGenImpl(
                domain, innerDomainPtr, numberValsRequired, neighbourhoods);
        },
        domain.to);
}

template <typename PreimageDomain>
struct FunctionAdd
    : public NeighbourhoodFunc<FunctionDomain, 1, FunctionAdd<PreimageDomain>> {
    typedef
        typename AssociatedValueType<PreimageDomain>::type PreimageValueType;
    typedef
        typename AssociatedViewType<PreimageValueType>::type PreimageViewType;
    const FunctionDomain& domain;
    const PreimageDomain& preimageDomain;
    const UInt preimageDomainSize;
    FunctionAdd(const FunctionDomain& domain)
        : domain(domain),
          preimageDomain(*lib::get<shared_ptr<PreimageDomain>>(domain.from)),
          preimageDomainSize(getDomainSize(preimageDomain)) {}
    static string getName() { return "FunctionAdd"; }
    static bool matches(const FunctionDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE &&
               domain.partial != PartialAttr::TOTAL;
    }
    void apply(NeighbourhoodParams& params, FunctionValue& val) {
        if (val.rangeSize() == domain.sizeAttr.maxSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        lib::visit(
            [&](auto& imageDomainPtr) {
                auto& imageDomain = *imageDomainPtr;
                typedef BaseType<decltype(imageDomain)> ImageDomain;
                typedef typename AssociatedValueType<ImageDomain>::type
                    ImageValueType;

                int numberTries = 0;
                const int tryLimit = params.parentCheckTryLimit *
                                     calcNumberInsertionAttempts(
                                         val.rangeSize(), preimageDomainSize);
                debug_neighbourhood_action("Looking for value to add");
                bool success = false;
                TupleDomain combinedDomain({domain.from, domain.to});
                NeighbourhoodResourceAllocator resourceAllocator(
                    combinedDomain);
                auto newMember = constructValueFromDomain(combinedDomain);

                do {
                    auto resource = resourceAllocator.requestLargerResource();
                    success = assignRandomValueInDomain(combinedDomain,
                                                        *newMember, resource);
                    params.stats.minorNodeCount +=
                        resource.getResourceConsumed();
                    success = success &&
                              !val.getPreimages().preimageHashIndexMap.count(
                                  getValueHash(newMember->members[0]));
                    if (!success) {
                        continue;
                    }
                    success = val.tryAddValue(
                        newMember->member<PreimageValueType>(0),
                        newMember->member<ImageValueType>(1),
                        [&]() { return params.parentCheck(params.vals); });
                } while (!success && ++numberTries < tryLimit);
                if (!success) {
                    debug_neighbourhood_action(
                        "Couldn't find value, number tries=" << tryLimit);
                    return;
                }
                debug_neighbourhood_action("Added value: " << newMember);
                if (!params.changeAccepted()) {
                    debug_neighbourhood_action("Change rejected");
                    val.tryRemoveValue<PreimageValueType, ImageValueType>(
                        val.rangeSize() - 1, []() { return true; });
                }
            },
            domain.to);
    }
};

template <typename PreimageDomain>
struct FunctionRemove
    : public NeighbourhoodFunc<FunctionDomain, 1,
                               FunctionRemove<PreimageDomain>> {
    typedef
        typename AssociatedValueType<PreimageDomain>::type PreimageValueType;
    typedef
        typename AssociatedViewType<PreimageValueType>::type PreimageViewType;

    const FunctionDomain& domain;
    FunctionRemove(const FunctionDomain& domain) : domain(domain) {}
    static string getName() { return "FunctionRemove"; }
    static bool matches(const FunctionDomain& domain) {
        return domain.sizeAttr.sizeType != SizeAttr::SizeAttrType::EXACT_SIZE &&
               domain.partial != PartialAttr::TOTAL;
    }
    void apply(NeighbourhoodParams& params, FunctionValue& val) {
        if (val.rangeSize() == domain.sizeAttr.minSize) {
            ++params.stats.minorNodeCount;
            return;
        }
        lib::visit(
            [&](auto& imageDomainPtr) {
                auto& imageDomain = *imageDomainPtr;
                typedef BaseType<decltype(imageDomain)> ImageDomain;
                typedef typename AssociatedValueType<ImageDomain>::type
                    ImageValueType;

                size_t indexToRemove;
                int numberTries = 0;
                lib::optional<
                    pair<ValRef<PreimageValueType>, ValRef<ImageValueType>>>
                    removedMember;
                bool success = false;
                debug_neighbourhood_action("Looking for value to remove");
                do {
                    ++params.stats.minorNodeCount;
                    indexToRemove =
                        globalRandom<size_t>(0, val.rangeSize() - 1);
                    debug_log("trying to remove index " << indexToRemove
                                                        << " from Function "
                                                        << val.view());
                    removedMember =
                        val.tryRemoveValue<PreimageValueType, ImageValueType>(
                            indexToRemove,
                            [&]() { return params.parentCheck(params.vals); });
                    success = removedMember.has_value();
                    if (success) {
                        debug_neighbourhood_action("Removed "
                                                   << *removedMember);
                    }
                } while (!success &&
                         ++numberTries < params.parentCheckTryLimit);
                if (!success) {
                    debug_neighbourhood_action(
                        "Couldn't find value, number tries=" << numberTries);
                    return;
                }
                if (!params.changeAccepted()) {
                    debug_neighbourhood_action("Change rejected");
                    val.tryAddValue(removedMember->first, removedMember->second,
                                    []() { return true; });
                }
            },
            domain.to);
    }
};

template <typename InnerDomain>
struct FunctionImagesSwap
    : public NeighbourhoodFunc<FunctionDomain, 1,
                               FunctionImagesSwap<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const FunctionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    FunctionImagesSwap(const FunctionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.to)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "FunctionImagesSwap"; }
    static bool matches(const FunctionDomain&) { return true; }
    void apply(NeighbourhoodParams& params, FunctionValue& val) {
        if (val.rangeSize() < 2) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Looking for indices to swap");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id).childViolations(
                FunctionValue::IMAGE_VALBASE_INDEX);

        bool success;
        UInt index1, index2;
        do {
            success = false;
            ++params.stats.minorNodeCount;
            index1 =
                vioContainerAtThisLevel.selectRandomVar(val.rangeSize() - 1);
            index2 = globalRandom<UInt>(0, val.rangeSize() - 1);
            index2 = (index1 + index2) % val.rangeSize();
            if (index2 < index1) {
                swap(index1, index2);
            }
            success = val.trySwapImages<InnerValueType>(index1, index2, [&]() {
                return params.parentCheck(params.vals);
            });
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find positions to swap, number tries=" << tryLimit);
            return;
        }
        debug_neighbourhood_action("positions swapped: " << index1 << " and "
                                                         << index2);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.trySwapImages<InnerValueType>(index1, index2,
                                              []() { return true; });
        }
    }
};

Int getRandomValueInDomain(const IntDomain& domain);
// neighbourhood, when a functionmaps from tuple of int to something, treat as
// coordinates and only swap along same row, column, etc.
template <typename InnerDomain>
struct FunctionImagesSwapAlongAxis
    : public NeighbourhoodFunc<FunctionDomain, 1,
                               FunctionImagesSwapAlongAxis<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const FunctionDomain& domain;
    const InnerDomain& innerDomain;
    shared_ptr<TupleDomain> preimageDomain;

    FunctionImagesSwapAlongAxis(const FunctionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.to)),
          preimageDomain(lib::get<shared_ptr<TupleDomain>>(this->domain.from)) {
    }

    static string getName() { return "FunctionImagesSwapAlongAxis"; }
    static bool matches(const FunctionDomain& domain) {
        auto tupleDomain = lib::get_if<shared_ptr<TupleDomain>>(&domain.from);
        if (!tupleDomain) {
            return false;
        }
        if ((**tupleDomain).inners.size() < 2) {
            return false;
        }
        for (auto& innerDomain : (**tupleDomain).inners) {
            if (!lib::get_if<shared_ptr<IntDomain>>(&innerDomain)) {
                return false;
            }
        }
        return true;
    }
    void apply(NeighbourhoodParams& params, FunctionValue& val) {
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Looking for indices to swap");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id).childViolations(
                FunctionValue::IMAGE_VALBASE_INDEX);

        bool success;
        UInt index1, index2;
        do {
            success = false;
            ++params.stats.minorNodeCount;
            lib::optional<pair<UInt, UInt>> indicesToSwap =
                getIndicesToSwap(val, vioContainerAtThisLevel);
            if (!indicesToSwap) {
                continue;
            }
            tie(index1, index2) = *indicesToSwap;
            if (index2 < index1) {
                swap(index1, index2);
            }

            success = val.trySwapImages<InnerValueType>(index1, index2, [&]() {
                return params.parentCheck(params.vals);
            });
        } while (!success && ++numberTries < tryLimit);
        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find positions to swap, number tries=" << tryLimit);
            return;
        }
        debug_neighbourhood_action("positions swapped: " << index1 << " and "
                                                         << index2);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            val.trySwapImages<InnerValueType>(index1, index2,
                                              []() { return true; });
        }
    }

    lib::optional<pair<UInt, UInt>> getIndicesToSwap(
        FunctionValue& val, ViolationContainer& vioContainer) {
        // first, choose an index, biasing towards violating members
        UInt index1 = vioContainer.selectRandomVar(val.rangeSize() - 1);
        // now we need the second index.  For this we need to first randomly
        // choose the dimension we what to swap in. extract preimage that is
        // this index
        auto preimage = assumeAsValue(val.indexToPreimage<TupleDomain>(index1));
        size_t dimToChange =
            globalRandom<size_t>(0, preimage->members.size() - 1);
        auto intVal = preimage->template member<IntValue>(dimToChange);
        auto& intDomain = lib::get<shared_ptr<IntDomain>>(
            preimageDomain->inners[dimToChange]);
        if (getDomainSize(*intDomain) < 2) {
            return lib::nullopt;
        }
        // find a different value
        Int newValue;
        do {
            newValue = getRandomValueInDomain(*intDomain);
        } while (newValue == intVal->value);
        intVal->value = newValue;
        auto index2 = val.preimageToIndex(*preimage);
        debug_code(assert(index2));
        return make_pair(index1, *index2);
    }
};

Int& valueOf(IntView& v) { return v.value; }
UInt& valueOf(BoolView& v) { return v.violation; }

template <typename InnerDomain>
struct FunctionUnifyImages
    : public NeighbourhoodFunc<FunctionDomain, 1,
                               FunctionUnifyImages<InnerDomain>> {
    typedef InnerDomain InnerDomainType;
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    typedef BaseType<decltype(valueOf(declval<InnerViewType&>()))> ValueType;
    const FunctionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    FunctionUnifyImages(const FunctionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.to)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "FunctionUnifyImages"; }
    static bool matches(const FunctionDomain&) { return true; }
    void apply(NeighbourhoodParams& params, FunctionValue& val) {
        if (val.rangeSize() < 2) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit =
            params.parentCheckTryLimit *
            calcNumberInsertionAttempts(val.rangeSize(), innerDomainSize);
        debug_neighbourhood_action("Looking for indices to unitfy");

        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id).childViolations(
                FunctionValue::IMAGE_VALBASE_INDEX);

        bool success;
        UInt index1, index2;
        ValueType valueBackup;
        do {
            success = false;
            ++params.stats.minorNodeCount;
            ++params.stats.minorNodeCount;
            index1 =
                vioContainerAtThisLevel.selectRandomVar(val.rangeSize() - 1);
            index2 = globalRandom<UInt>(0, val.rangeSize() - 1);
            index2 = (index1 + index2) % val.rangeSize();

            if (valueOf(*val.getImageVal<InnerValueType>(index1)) ==
                valueOf(*val.getImageVal<InnerValueType>(index2))) {
                continue;
            }
            // randomly swap indices
            if (globalRandom(0, 1)) {
                swap(index1, index2);
            }
            auto& view1 = val.getRange<InnerViewType>()[index1]->view().get();
            auto& view2 = val.getRange<InnerViewType>()[index2]->view().get();
            valueBackup = valueOf(view2);
            success = view2.changeValue([&]() {
                valueOf(view2) = valueOf(view1);
                bool allowed = val.tryImageChange<IntValue>(
                    index2, [&]() { return params.parentCheck(params.vals); });
                if (allowed) {
                    return true;
                } else {
                    valueOf(view2) = valueBackup;
                    return false;
                }
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
            auto& view2 = val.getRange<InnerViewType>()[index2]->view().get();
            view2.changeValue([&]() -> bool {
                valueOf(view2) = valueBackup;
                return val.tryImageChange<IntValue>(index2,
                                                    [&]() { return true; });
            });
        }
    }
};

template <typename InnerDomain>
struct FunctionSplitImages
    : public NeighbourhoodFunc<FunctionDomain, 1,
                               FunctionSplitImages<InnerDomain>> {
    typedef InnerDomain InnerDomainType;
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    typedef BaseType<decltype(valueOf(declval<InnerViewType&>()))> ValueType;
    const FunctionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    FunctionSplitImages(const FunctionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.to)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "FunctionSplitImages"; }
    static bool matches(const FunctionDomain&) { return true; }
    void apply(NeighbourhoodParams& params, FunctionValue& val) {
        if (val.rangeSize() < 2) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit =
            params.parentCheckTryLimit *
            calcNumberInsertionAttempts(val.rangeSize(), innerDomainSize);
        debug_neighbourhood_action("Looking for indices to split");
        auto& vioContainerAtThisLevel =
            params.vioContainer.childViolations(val.id).childViolations(
                FunctionValue::IMAGE_VALBASE_INDEX);

        bool success;
        UInt index1, index2;
        ValueType valueBackup;
        NeighbourhoodResourceAllocator allocator(innerDomain);
        do {
            success = false;

            ++params.stats.minorNodeCount;
            index1 =
                vioContainerAtThisLevel.selectRandomVar(val.rangeSize() - 1);
            index2 = globalRandom<UInt>(0, val.rangeSize() - 1);
            index2 = (index1 + index2) % val.rangeSize();

            if (valueOf(*val.getImageVal<InnerValueType>(index1)) !=
                valueOf(*val.getImageVal<InnerValueType>(index2))) {
                continue;
            }
            // randomly swap indices
            if (globalRandom(0, 1)) {
                swap(index1, index2);
            }
            auto& view1 = val.getRange<InnerViewType>()[index1]->view().get();
            static_cast<void>(view1);
            auto value2 = val.getImageVal<InnerValueType>(index2);

            valueBackup = valueOf(*value2);
            auto resource = allocator.requestLargerResource();
            success = value2->changeValue([&]() {
                bool assigned =
                    assignRandomValueInDomain(innerDomain, *value2, resource);
                bool allowed =
                    assigned && valueOf(*value2) != valueBackup &&
                    val.tryImageChange<InnerValueType>(index2, [&]() {
                        return params.parentCheck(params.vals);
                    });
                if (!allowed) {
                    valueOf(*value2) = valueBackup;
                }
                return allowed;
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
            auto& view2 = val.getRange<InnerViewType>()[index2]->view().get();
            view2.changeValue([&]() {
                valueOf(view2) = valueBackup;
                return val.tryImageChange<InnerValueType>(
                    index2, [&]() { return true; });
            });
        }
    }
};

template <typename InnerDomain>
struct FunctionAssignRandom
    : public NeighbourhoodFunc<FunctionDomain, 1,
                               FunctionAssignRandom<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;

    const FunctionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    FunctionAssignRandom(const FunctionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.to)),
          innerDomainSize(getDomainSize(innerDomain)) {}

    static string getName() { return "FunctionAssignRandom"; }
    static bool matches(const FunctionDomain&) { return true; }
    void apply(NeighbourhoodParams& params, FunctionValue& val) {
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Assigning random value: original value is "
                                   << asView(val));
        auto backup = deepCopy(val);
        backup->container = val.container;
        auto newValue = constructValueFromDomain(domain);
        newValue->container = val.container;
        bool success;
        NeighbourhoodResourceAllocator allocator(domain);
        do {
            success = false;
            auto resource = allocator.requestLargerResource();
            success = assignRandomValueInDomain(domain, *newValue, resource);
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
    }
};
template <typename InnerDomain>
struct FunctionCrossover
    : public NeighbourhoodFunc<FunctionDomain, 2,
                               FunctionCrossover<InnerDomain>> {
    typedef typename AssociatedValueType<InnerDomain>::type InnerValueType;
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
    const FunctionDomain& domain;
    const InnerDomain& innerDomain;
    const UInt innerDomainSize;
    FunctionCrossover(const FunctionDomain& domain)
        : domain(domain),
          innerDomain(*lib::get<shared_ptr<InnerDomain>>(domain.to)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "FunctionCrossover"; }
    static bool matches(const FunctionDomain&) { return true; }

    template <typename InnerValueType, typename Func>
    static bool performCrossOver(FunctionValue& fromVal, FunctionValue& toVal,
                                 UInt indexToCrossOver,
                                 ValRef<InnerValueType> member1,
                                 ValRef<InnerValueType> member2,
                                 Func&& parentCheck) {
        swapValAssignments(*member1, *member2);
        bool success =
            fromVal.tryImageChange<InnerValueType>(indexToCrossOver, [&]() {
                return toVal.tryImageChange<InnerValueType>(
                    indexToCrossOver, [&]() {
                        if (parentCheck()) {
                            return true;
                        } else {
                            swapValAssignments(*member1, *member2);
                            return false;
                        }
                    });
            });
        return success;
    }

    void apply(NeighbourhoodParams& params, FunctionValue& fromVal,
               FunctionValue& toVal) {
        if (fromVal.rangeSize() == 0 || toVal.rangeSize() == 0) {
            ++params.stats.minorNodeCount;
            return;
        }
        int numberTries = 0;
        const int tryLimit = params.parentCheckTryLimit;
        debug_neighbourhood_action("Looking for values to cross over");
        bool success = false;
        UInt indexToCrossOver;
        const UInt maxCrossOverIndex =
            min(fromVal.rangeSize(), toVal.rangeSize()) - 1;
        ValRef<InnerValueType> member1 = nullptr, member2 = nullptr;
        do {
            success = false;
            ++params.stats.minorNodeCount;
            indexToCrossOver = globalRandom<UInt>(0, maxCrossOverIndex);
            member1 = fromVal.getImageVal<InnerValueType>(indexToCrossOver);
            member2 = toVal.getImageVal<InnerValueType>(indexToCrossOver);
            success = performCrossOver(
                fromVal, toVal, indexToCrossOver, member1, member2,
                [&]() { return params.parentCheck(params.vals); });
        } while (!success && ++numberTries < tryLimit);

        if (!success) {
            debug_neighbourhood_action(
                "Couldn't find values to cross over, number tries="
                << tryLimit);
            return;
        }

        debug_neighbourhood_action(
            "CrossOverd values: "
            << toVal.getRange<InnerViewType>()[indexToCrossOver] << " and "
            << fromVal.getRange<InnerViewType>()[indexToCrossOver]);
        if (!params.changeAccepted()) {
            debug_neighbourhood_action("Change rejected");
            performCrossOver(fromVal, toVal, indexToCrossOver, member1, member2,
                             [&]() { return true; });
        }
    }
};

struct ImageDomainGetter {
    const AnyDomainRef operator()(const FunctionDomain& domain) {
        return domain.to;
    }
};

struct PreimageDomainGetter {
    const AnyDomainRef operator()(const FunctionDomain& domain) {
        return domain.from;
    }
};

Neighbourhood generateRandomReassignNeighbourhood(
    const FunctionDomain& domain) {
    return lib::visit(
        [&](auto& innerDomainImpl) {
            typedef typename BaseType<decltype(innerDomainImpl)>::element_type
                InnerDomainType;
            return Neighbourhood("FunctionAssignRandomDedicated", 1,
                                 FunctionAssignRandom<InnerDomainType>(domain));
        },
        domain.to);
}

const NeighbourhoodVec<FunctionDomain>
    NeighbourhoodGenList<FunctionDomain>::value = {
        {1, functionLiftImageSingleGen},       //
#ifndef REDUCED_NS
        {1, functionLiftImageMultipleGen},     //
#endif
        {1, functionLiftPreimageSingleGen},    //
#ifndef REDUCED_NS
        {1, functionLiftPreimageMultipleGen},  //
#endif
        {1, generateForAllTypes<FunctionDomain, FunctionImagesSwap,
                                ImageDomainGetter>},  //
        {1, generateForAllTypes<FunctionDomain, FunctionAdd,
                                PreimageDomainGetter>},  //
        {1, generateForAllTypes<FunctionDomain, FunctionRemove,
                                PreimageDomainGetter>},  //
#ifndef REDUCED_NS
        {1, generateForAllTypes<FunctionDomain, FunctionImagesSwapAlongAxis,
                                ImageDomainGetter>},  //
        {2, generateForAllTypes<FunctionDomain, FunctionCrossover,
                                ImageDomainGetter>},  //
        {1, generate<FunctionDomain, FunctionUnifyImages<IntDomain>,
                     ImageDomainGetter>},  //
        {1, generate<FunctionDomain, FunctionUnifyImages<BoolDomain>,
                     ImageDomainGetter>},  //
        {1, generate<FunctionDomain, FunctionSplitImages<IntDomain>,
                     ImageDomainGetter>},  //
        {1, generate<FunctionDomain, FunctionSplitImages<BoolDomain>,
                     ImageDomainGetter>},  //
#endif
};
