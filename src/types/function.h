#ifndef SRC_TYPES_FUNCTION_H_
#define SRC_TYPES_FUNCTION_H_
#include <vector>

#include "base/base.h"
#include "common/common.h"
#include "triggers/functionTrigger.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

static const char* NO_FUNCTION_UNDEFINED_MEMBERS =
    "Not yet handling functions with undefined members.\n";
bool canBuildDimensionVec(const AnyDomainRef& domain);

// pre images are either explicitly stored or are inferred from just the index
// of the image.  For explicitly stored pre images, we use the preimage
// container.  For inferred pre images, we use dimensions struct, which can be
// used to convert pre images to indices and vice versa.
struct Dimension {
    // dimensions are like int domain bounds, lower and upper inclusive
    Int lower;
    Int upper;
    UInt blockSize = 0;
    Dimension(Int lower, Int upper) : lower(lower), upper(upper) {}

    inline bool operator==(const Dimension& other) const {
        return lower == other.lower && upper == other.upper &&
               blockSize == other.blockSize;
    }
    friend inline std::ostream& operator<<(std::ostream& os,
                                           const Dimension& d) {
        return os << "Dimension(lower=" << d.lower << ", upper=" << d.upper
                  << ", blockSize=" << d.blockSize << ")";
    }
};

typedef std::vector<Dimension> DimensionVec;

struct ExplicitPreimageContainer {
    AnyExprVec preimages;
    std::vector<HashType> preimageHashes;
    HashMap<HashType, UInt> preimageHashIndexMap;
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& get() {
        return lib::get<ExprRefVec<InnerViewType>>(preimages);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& get() const {
        return lib::get<ExprRefVec<InnerViewType>>(preimages);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool add(ExprRef<InnerViewType> expr) {
        auto hash = getValueHash(expr);
        if (preimageHashIndexMap.count(hash)) {
            return false;
        }
        get<InnerViewType>().emplace_back(expr);
        preimageHashes.emplace_back(hash);
        preimageHashIndexMap[hash] = preimageHashes.size() - 1;
        return true;
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    ExprRef<InnerViewType> remove(UInt index) {
        auto& preimages = this->get<InnerViewType>();
        debug_code(assert(index < preimages.size()));
        if (index < preimages.size() - 1) {
            std::swap(preimages[index], preimages.back());
            std::swap(preimageHashes[index], preimageHashes.back());
            std::swap(preimageHashIndexMap.at(preimageHashes[index]),
                      preimageHashIndexMap.at(preimageHashes.back()));
        }
        auto hash = preimageHashes.back();
        auto expr = preimages.back();
        preimageHashes.pop_back();
        preimages.pop_back();
        preimageHashIndexMap.erase(hash);
        return expr;
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void swap(UInt index1, UInt index2) {
        auto& preimages = this->get<InnerViewType>();
        debug_code(assert(index1 < preimages.size()));
        debug_code(assert(index2 < preimages.size()));
        std::swap(preimages[index1], preimages[index2]);
        std::swap(preimageHashes[index1], preimageHashes[index2]);
        std::swap(preimageHashIndexMap.at(preimageHashes[index1]),
                  preimageHashIndexMap.at(preimageHashes[index2]));
    }

    void change(UInt index, HashType newHash) {
        debug_code(assert(index < preimageHashes.size()));
        preimageHashIndexMap.erase(preimageHashes[index]);
        preimageHashes[index] = newHash;
        preimageHashIndexMap[newHash] = index;
    }
};
struct Uninit {};
typedef lib::variant<DimensionVec, ExplicitPreimageContainer, Uninit> Preimages;

struct CachedHashes {
    HashType hashTotal = HashType(0);  // hash of entire function
    std::vector<HashType> rangeHashes;
    // these functions don't do much yet, they are thin wrappers, but they are
    // there as we may have more complicated hash structures, such as a
    // hashIndexMap
    inline void assignRangeHash(UInt index, HashType newHash,
                                bool replaceOld = true) {
        ignoreUnused(replaceOld);
        debug_code(assert(index < rangeHashes.size()));
        rangeHashes[index] = newHash;
    }
    inline void swapRangeHashes(UInt index1, UInt index2) {
        debug_code(assert(index1 < rangeHashes.size()));
        debug_code(assert(index2 < rangeHashes.size()));
        std::swap(rangeHashes[index1], rangeHashes[index2]);
    }

    inline void unassignRangeHash(UInt index) {
        ignoreUnused(index);
        debug_code(assert(index < rangeHashes.size()));
    }
    void addNewHash(HashType hash) { rangeHashes.emplace_back(hash); }
    HashType removeHash(UInt index) {
        HashType removedHash = rangeHashes[index];
        rangeHashes[index] = rangeHashes.back();
        rangeHashes.pop_back();
        return removedHash;
    }
};

DimensionVec makeDimensionVecFromDomain(const AnyDomainRef& domain);
template <typename Domain>
ExprRef<typename AssociatedViewType<
    typename AssociatedValueType<Domain>::type>::type>
functionIndexToPreimage(const Domain&, const DimensionVec&, UInt, size_t&) {
    todoImpl();
}

template <>
ExprRef<IntView> functionIndexToPreimage<IntDomain>(
    const IntDomain& domain, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex);

template <>
ExprRef<EnumView> functionIndexToPreimage<EnumDomain>(
    const EnumDomain& domain, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex);

template <>
ExprRef<TupleView> functionIndexToPreimage<TupleDomain>(
    const TupleDomain&, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex);

template <typename Domain>
void functionIndexToPreimage(
    const Domain&, const DimensionVec&, UInt,
    typename AssociatedViewType<
        typename AssociatedValueType<Domain>::type>::type&,
    size_t&) {
    todoImpl();
}

template <>
void functionIndexToPreimage<IntDomain>(const IntDomain&,
                                        const DimensionVec& dimensions,
                                        UInt index, IntView&, size_t& dimIndex);

template <>
void functionIndexToPreimage<EnumDomain>(const EnumDomain&,
                                         const DimensionVec& dimensions,
                                         UInt index, EnumView&,
                                         size_t& dimIndex);

template <>
void functionIndexToPreimage<TupleDomain>(const TupleDomain&,
                                          const DimensionVec& dimensions, UInt,
                                          TupleView&, size_t& index);

lib::optional<Int> getAsIntForFunctionIndex(const AnyExprRef& expr);

struct FunctionView : public ExprInterface<FunctionView>,
                      TriggerContainer<FunctionView> {
    friend FunctionValue;
    AnyDomainRef preimageDomain;

    Preimages preimages = Uninit();
    AnyExprVec range;
    bool partial = false;
    SimpleCache<CachedHashes> cachedHashes;
    UInt numberUndefined = 0;

    bool lazyPreimages() const { return lib::get_if<DimensionVec>(&preimages); }
    const DimensionVec& getDimensions() const {
        return lib::get<DimensionVec>(preimages);
    }

    ExplicitPreimageContainer& getPreimages() {
        return lib::get<ExplicitPreimageContainer>(preimages);
    }

    const ExplicitPreimageContainer& getPreimages() const {
        return lib::get<ExplicitPreimageContainer>(preimages);
    }
    FunctionView() {}

    void initView(AnyDomainRef preImageDomain, Preimages preimages,
                  AnyExprVec range, bool partial) {
        cachedHashes.invalidate();
        this->partial = partial;
        this->preimageDomain = preImageDomain;
        this->preimages = std::move(preimages);
        this->range = std::move(range);
    }

    lib::optional<UInt> preimageToIndex(const IntView& intV);
    lib::optional<UInt> preimageToIndex(const EnumView& tupleV);
    lib::optional<UInt> preimageToIndex(const TupleView& tupleV);

    template <typename View, EnableIfView<BaseType<View>> = 0>
    lib::optional<UInt> preimageToIndex(const View& view) {
        return nonLazyDomainToIndex(view);
    }

    template <typename View, EnableIfView<BaseType<View>> = 0>
    lib::optional<UInt> nonLazyDomainToIndex(const View& view) const {
        const auto& map = getPreimages().preimageHashIndexMap;
        auto iter = map.find(getValueHash(view));
        if (iter != map.end()) {
            return iter->second;
        } else {
            return lib::nullopt;
        }
    }
    template <typename Domain,
              typename View = typename AssociatedViewType<Domain>::type>
    ExprRef<View> indexToPreimage(UInt index) const {
        if (!lazyPreimages()) {
            debug_code(assert(index < rangeSize()));
            return getPreimages().get<View>()[index];
        }
        size_t dimIndex = 0;
        return functionIndexToPreimage<Domain>(
            *lib::get<std::shared_ptr<Domain>>(preimageDomain), getDimensions(),
            index, dimIndex);
    }

    void debugCheckDimensionVec();

    HashType getPreimageHash(UInt index) const {
        if (!lazyPreimages()) {
            auto& preimages = getPreimages();
            debug_code(assert(index < preimages.preimageHashes.size()));
            return preimages.preimageHashes[index];
        }
        return lib::visit(
            [&](auto& domain) {
                typedef
                    typename BaseType<decltype(domain)>::element_type Domain;
                return getValueHash(indexToPreimage<Domain>(index));
            },
            preimageDomain);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType calcMemberHash(UInt index,
                                   const ExprRef<InnerViewType>& expr) const {
        HashType input[2];
        input[0] = getPreimageHash(index);
        input[1] = getValueHash(
            expr->view().checkedGet(NO_FUNCTION_UNDEFINED_MEMBERS));
        return mix(((char*)input), sizeof(input));
    }

    inline HashType calcMemberHashFromCache(
        UInt index, const CachedHashes& cachedHashes) const {
        debug_code(assert(index < cachedHashes.rangeHashes.size()));
        HashType input[2];
        input[0] = getPreimageHash(index);
        input[1] = cachedHashes.rangeHashes[index];
        return mix(((char*)input), sizeof(input));
    }
    template <typename InnerViewType>
    inline void imageChanged(UInt index) {
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.hashTotal -=
                calcMemberHashFromCache(index, cachedHashes);
            cachedHashes.assignRangeHash(
                index, getValueHash(getRange<InnerViewType>()[index]));
            cachedHashes.hashTotal +=
                calcMemberHashFromCache(index, cachedHashes);
        });
    }

    template <typename InnerViewType>
    inline void preimageChanged(UInt index) {
        auto& preimages = getPreimages();
        debug_code(assert(index < rangeSize()));
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.hashTotal -=
                calcMemberHashFromCache(index, cachedHashes);
        });
        auto newHash = getValueHash(preimages.get<InnerViewType>()[index]);
        preimages.change(index, newHash);
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.hashTotal +=
                calcMemberHashFromCache(index, cachedHashes);
        });
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void imagesChanged(const std::vector<UInt>& indices) {
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            for (auto index : indices) {
                cachedHashes.hashTotal -=
                    calcMemberHashFromCache(index, cachedHashes);
                cachedHashes.assignRangeHash(
                    index, getValueHash(getRange<InnerViewType>()[index]));
                cachedHashes.hashTotal +=
                    calcMemberHashFromCache(index, cachedHashes);
            }
        });
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void preimagesChanged(const std::vector<UInt>& indices) {
        auto& preimages = getPreimages();
        for (auto index : indices) {
            debug_code(assert(index < rangeSize()));
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                cachedHashes.hashTotal -=
                    calcMemberHashFromCache(index, cachedHashes);
            });
            auto newHash = getValueHash(preimages.get<InnerViewType>()[index]);
            preimages.change(index, newHash);
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                cachedHashes.hashTotal +=
                    calcMemberHashFromCache(index, cachedHashes);
            });
        }
    }

    template <typename PreimageViewType, typename ImageViewType,
              EnableIfView<PreimageViewType> = 0,
              EnableIfView<ImageViewType> = 0>
    inline bool addValue(const ExprRef<PreimageViewType>& preimage,
                         const ExprRef<ImageViewType>& image) {
        auto& preimages = getPreimages();
        if (!preimages.add(preimage)) {
            return false;
        }
        getRange<ImageViewType>().emplace_back(image);
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            auto hash = getValueHash(image);
            cachedHashes.addNewHash(hash);
            cachedHashes.hashTotal +=
                calcMemberHashFromCache(rangeSize() - 1, cachedHashes);
        });
        return true;
    }
    template <typename PreimageViewType, typename ImageViewType,
              EnableIfView<PreimageViewType> = 0,
              EnableIfView<ImageViewType> = 0>
    inline void removeValue(UInt index) {
        auto& preimages = getPreimages();
        debug_code(assert(index < rangeSize()));
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.hashTotal -=
                calcMemberHashFromCache(index, cachedHashes);
            cachedHashes.removeHash(index);
        });
        preimages.remove<PreimageViewType>(index);
        auto& range = getRange<ImageViewType>();
        range[index] = std::move(range.back());
        range.pop_back();
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void swapImages(UInt index1, UInt index2) {
        auto& range = getRange<InnerViewType>();
        std::swap(range[index1], range[index2]);
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.hashTotal -=
                calcMemberHashFromCache(index1, cachedHashes);
            cachedHashes.hashTotal -=
                calcMemberHashFromCache(index2, cachedHashes);
            cachedHashes.swapRangeHashes(index1, index2);
            cachedHashes.hashTotal +=
                calcMemberHashFromCache(index1, cachedHashes);
            cachedHashes.hashTotal +=
                calcMemberHashFromCache(index2, cachedHashes);
        });
        debug_code(standardSanityChecksForThisType());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void defineMemberAndNotify(UInt index) {
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.assignRangeHash(
                index, getValueHash(getRange<InnerViewType>()[index]), false);
            cachedHashes.hashTotal +=
                calcMemberHashFromCache(index, cachedHashes);
        });
        debug_code(assert(numberUndefined > 0));
        numberUndefined--;
        if (numberUndefined == 0) {
            this->setAppearsDefined(true);
        }
        notifyMemberDefined(index);
    }

    inline void undefineMemberAndNotify(UInt index) {
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.hashTotal -=
                calcMemberHashFromCache(index, cachedHashes);
            cachedHashes.unassignRangeHash(index);
        });
        numberUndefined++;
        this->setAppearsDefined(false);
        notifyMemberUndefined(index);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getRange() {
        return lib::get<ExprRefVec<InnerViewType>>(range);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getRange() const {
        return lib::get<ExprRefVec<InnerViewType>>(range);
    }

    virtual inline AnyExprVec& getChildrenOperands() { shouldNotBeCalledPanic; }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return getRange<InnerViewType>();
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return getRange<InnerViewType>();
    }

    inline UInt rangeSize() const {
        return lib::visit([](auto& range) { return range.size(); }, range);
    }

    void standardSanityChecksForThisType() const;

    void silentClear() {
        lib::visit(
            [&](auto& membersImpl) {
                cachedHashes.invalidate();
                membersImpl.clear();
                preimages = Uninit();
            },
            range);
    }
};

#endif /* SRC_TYPES_FUNCTION_H_ */
