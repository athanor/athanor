#include <cmath>
#include <random>
#include "neighbourhoods/neighbourhoods.h"
#include "search/statsContainer.h"
#include "types/boolVal.h"
#include "types/functionVal.h"
#include "types/intVal.h"
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
        domain.from, FunctionView::makeDimensionVecFromDomain(domain.from));
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
    typedef typename AssociatedViewType<
        typename InnerDomainPtrType::element_type>::type InnerViewType;
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
                auto previousMemberHash =
                    val.notifyPossibleImageChange<InnerViewType>(indexToChange);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val.tryImageChange<InnerValueType>(
                        indexToChange, previousMemberHash,
                        [&]() { return params.parentCheck(params.vals); });
                };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        previousMemberHash =
                            val.notifyPossibleImageChange<InnerViewType>(
                                indexToChange);
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
                                                       previousMemberHash,
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
    typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
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
                auto previousMembersCombinedHash =
                    val.notifyPossibleImagesChange<InnerViewType>(
                        indicesToChange);
                ParentCheckCallBack parentCheck = [&](const AnyValVec&) {
                    return val.tryImagesChange<InnerValueType>(
                        indicesToChange, previousMembersCombinedHash,
                        [&]() { return params.parentCheck(params.vals); });
                };
                bool requiresRevert = false;
                AcceptanceCallBack changeAccepted = [&]() {
                    requiresRevert = !params.changeAccepted();
                    if (requiresRevert) {
                        previousMembersCombinedHash =
                            val.notifyPossibleImagesChange<InnerViewType>(
                                indicesToChange);
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
                    val.tryImagesChange<InnerValueType>(
                        indicesToChange, previousMembersCombinedHash,
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
          innerDomain(*mpark::get<shared_ptr<InnerDomain>>(domain.to)),
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
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index1, index2;
        do {
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
          innerDomain(*mpark::get<shared_ptr<InnerDomain>>(domain.to)),
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
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index1, index2;
        ValueType valueBackup;
        lib::optional<HashType> previousMemberHash;
        do {
            ++params.stats.minorNodeCount;
            ++params.stats.minorNodeCount;
            index1 =
                vioContainerAtThisLevel.selectRandomVar(val.rangeSize() - 1);
            index2 = globalRandom<UInt>(0, val.rangeSize() - 1);
            index2 = (index1 + index2) % val.rangeSize();

            if (valueOf(*val.member<InnerValueType>(index1)) ==
                valueOf(*val.member<InnerValueType>(index2))) {
                continue;
            }
            // randomly swap indices
            if (globalRandom(0, 1)) {
                swap(index1, index2);
            }
            auto& view1 = val.getRange<IntView>()[index1]->view().get();
            auto& view2 = val.getRange<IntView>()[index2]->view().get();
            previousMemberHash =
                val.notifyPossibleImageChange<InnerViewType>(index2);
            valueBackup = view2.value;
            success = view2.changeValue([&]() {
                view2.value = view1.value;
                bool allowed = val.tryImageChange<IntValue>(
                    index2, previousMemberHash,
                    [&]() { return params.parentCheck(params.vals); });
                if (allowed) {
                    return true;
                } else {
                    view2.value = valueBackup;
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
            previousMemberHash =
                val.notifyPossibleImageChange<InnerViewType>(index2);
            auto& view2 = val.getRange<IntView>()[index2]->view().get();
            view2.changeValue([&]() -> bool {
                view2.value = valueBackup;
                return val.tryImageChange<IntValue>(index2, previousMemberHash,
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
          innerDomain(*mpark::get<shared_ptr<InnerDomain>>(domain.to)),
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
            params.vioContainer.childViolations(val.id);

        bool success;
        UInt index1, index2;
        ValueType valueBackup;
        lib::optional<HashType> previousMemberHash;
        do {
            ++params.stats.minorNodeCount;
            index1 =
                vioContainerAtThisLevel.selectRandomVar(val.rangeSize() - 1);
            index2 = globalRandom<UInt>(0, val.rangeSize() - 1);
            index2 = (index1 + index2) % val.rangeSize();

            if (valueOf(*val.member<InnerValueType>(index1)) !=
                valueOf(*val.member<InnerValueType>(index2))) {
                continue;
            }
            // randomly swap indices
            if (globalRandom(0, 1)) {
                swap(index1, index2);
            }
            auto& view1 = val.getRange<IntView>()[index1]->view().get();
            static_cast<void>(view1);
            auto value2 = val.member<InnerValueType>(index2);
            previousMemberHash =
                val.notifyPossibleImageChange<InnerViewType>(index2);
            valueBackup = valueOf(*value2);
            success = value2->changeValue([&]() {
                assignRandomValueInDomain(innerDomain, *value2, params.stats);
                bool allowed = valueOf(*value2) != valueBackup &&
                               val.tryImageChange<InnerValueType>(
                                   index2, previousMemberHash, [&]() {
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
            previousMemberHash =
                val.notifyPossibleImageChange<InnerViewType>(index2);
            auto& view2 = val.getRange<IntView>()[index2]->view().get();
            view2.changeValue([&]() {
                view2.value = valueBackup;
                return val.tryImageChange<InnerValueType>(
                    index2, previousMemberHash, [&]() { return true; });
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
          innerDomain(*mpark::get<shared_ptr<InnerDomain>>(domain.to)),
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
        do {
            assignRandomValueInDomain(domain, *newValue, params.stats);
            success = val.tryAssignNewValue(
                *newValue, [&]() { return params.parentCheck(params.vals); });
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
          innerDomain(*mpark::get<shared_ptr<InnerDomain>>(domain.to)),
          innerDomainSize(getDomainSize(innerDomain)) {}
    static string getName() { return "FunctionCrossover"; }
    static bool matches(const FunctionDomain&) { return true; }

    template <typename InnerValueType, typename Func>
    static bool performCrossOver(FunctionValue& fromVal, FunctionValue& toVal,
                                 UInt indexToCrossOver,
                                 ValRef<InnerValueType> member1,
                                 ValRef<InnerValueType> member2,
                                 Func&& parentCheck) {
        auto fromValPreviousMemberHash =
            fromVal.notifyPossibleImageChange<InnerViewType>(indexToCrossOver);
        auto toValPreviousMemberHash =
            toVal.notifyPossibleImageChange<InnerViewType>(indexToCrossOver);
        swapValAssignments(*member1, *member2);
        bool success = fromVal.tryImageChange<InnerValueType>(
            indexToCrossOver, fromValPreviousMemberHash, [&]() {
                return toVal.tryImageChange<InnerValueType>(
                    indexToCrossOver, toValPreviousMemberHash, [&]() {
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
            ++params.stats.minorNodeCount;
            indexToCrossOver = globalRandom<UInt>(0, maxCrossOverIndex);
            member1 = fromVal.member<InnerValueType>(indexToCrossOver);
            member2 = toVal.member<InnerValueType>(indexToCrossOver);
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

template <>
const AnyDomainRef getInner<FunctionDomain>(const FunctionDomain& domain) {
    return domain.to;
}

const NeighbourhoodVec<FunctionDomain>
    NeighbourhoodGenList<FunctionDomain>::value = {
        {1, functionLiftSingleGen},                                     //
        {1, functionLiftMultipleGen},                                   //
        {1, generateForAllTypes<FunctionDomain, FunctionImagesSwap>},   //
        {2, generateForAllTypes<FunctionDomain, FunctionCrossover>},    //
        {1, generate<FunctionDomain, FunctionUnifyImages<IntDomain>>},  //

        {1, generate<FunctionDomain, FunctionUnifyImages<BoolDomain>>},  //
        {1, generate<FunctionDomain, FunctionSplitImages<IntDomain>>},   //
        {1, generate<FunctionDomain, FunctionSplitImages<BoolDomain>>},  //
};
