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

struct Dimension {
    // dimensions are like int domain bounds, lower and upper inclusive
    Int lower;
    Int upper;
    UInt blockSize = 0;
    Dimension(Int lower, Int upper) : lower(lower), upper(upper) {}
};

typedef std::vector<Dimension> DimensionVec;

template <typename Domain>
ExprRef<typename AssociatedViewType<
    typename AssociatedValueType<Domain>::type>::type>
functionIndexToDomain(const Domain&, const DimensionVec&, UInt, size_t&) {
    todoImpl();
}

template <>
ExprRef<IntView> functionIndexToDomain<IntDomain>(
    const IntDomain& domain, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex);

template <>
ExprRef<EnumView> functionIndexToDomain<EnumDomain>(
    const EnumDomain& domain, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex);

template <>
ExprRef<TupleView> functionIndexToDomain<TupleDomain>(
    const TupleDomain&, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex);

template <typename Domain>
void functionIndexToDomain(
    const Domain&, const DimensionVec&, UInt,
    typename AssociatedViewType<
        typename AssociatedValueType<Domain>::type>::type&,
    size_t&) {
    todoImpl();
}

template <>
void functionIndexToDomain<IntDomain>(const IntDomain&,
                                      const DimensionVec& dimensions,
                                      UInt index, IntView&, size_t& dimIndex);

template <>
void functionIndexToDomain<EnumDomain>(const EnumDomain&,
                                       const DimensionVec& dimensions,
                                       UInt index, EnumView&, size_t& dimIndex);

template <>
void functionIndexToDomain<TupleDomain>(const TupleDomain&,
                                        const DimensionVec& dimensions, UInt,
                                        TupleView&, size_t& index);

lib::optional<Int> getAsIntForFunctionIndex(const AnyExprRef& expr);

struct FunctionView : public ExprInterface<FunctionView>,
                      TriggerContainer<FunctionView> {
    friend FunctionValue;
    AnyDomainRef fromDomain;
    DimensionVec dimensions;
    AnyExprVec range;
    SimpleCache<HashType> cachedHashTotal;

    lib::optional<UInt> domainToIndex(const IntView& intV);
    lib::optional<UInt> domainToIndex(const EnumView& tupleV);
    lib::optional<UInt> domainToIndex(const TupleView& tupleV);

    template <typename View, EnableIfView<BaseType<View>> = 0>
    lib::optional<UInt> domainToIndex(const View&) {
        shouldNotBeCalledPanic;
    }

    template <
        typename Domain,
        typename View = typename AssociatedViewType<
            typename AssociatedValueType<Domain>::type>::type,
        typename std::enable_if<IsDomainType<Domain>::value, int>::type = 0>
    void indexToDomain(UInt index, View& view) const {
        size_t dimIndex = 0;
        functionIndexToDomain<Domain>(
            *lib::get<std::shared_ptr<Domain>>(fromDomain), dimensions, index,
            view, dimIndex);
    }

    template <
        typename Domain,
        typename View = typename AssociatedViewType<
            typename AssociatedValueType<Domain>::type>::type,
        typename std::enable_if<IsDomainType<Domain>::value, int>::type = 0>
    ExprRef<View> indexToDomain(UInt index) const {
        size_t dimIndex = 0;
        return functionIndexToDomain<Domain>(
            *lib::get<std::shared_ptr<Domain>>(fromDomain), dimensions, index,
            dimIndex);
    }

    static DimensionVec makeDimensionVecFromDomain(const AnyDomainRef& domain);
    void debugCheckDimensionVec();
    template <typename InnerViewType, typename DimVec,
              EnableIfView<InnerViewType> = 0>
    void resetDimensions(AnyDomainRef fromDom, DimVec&& dim) {
        fromDomain = std::move(fromDom);
        dimensions = std::forward<DimVec>(dim);
        size_t requiredSize = 1;
        for (auto iter = dimensions.rbegin(); iter != dimensions.rend();
             ++iter) {
            auto& dim = *iter;
            dim.blockSize = requiredSize;
            requiredSize *= (dim.upper - dim.lower) + 1;
        }
        range.emplace<ExprRefVec<InnerViewType>>().assign(requiredSize,
                                                          nullptr);
        debug_code(debugCheckDimensionVec());
        ;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType calcMemberHash(UInt index,
                                   const ExprRef<InnerViewType>& expr) const {
        HashType input[2];
        input[0] = HashType(index);
        input[1] = getValueHash(
            expr->view().checkedGet(NO_FUNCTION_UNDEFINED_MEMBERS));
        return mix(((char*)input), sizeof(input));
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void assignImage(size_t index,
                            const ExprRef<InnerViewType>& member) {
        auto& range = getRange<InnerViewType>();
        debug_code(assert(index < range.size()));
        cachedHashTotal.applyIfValid([&](auto& value) {
            value -= this->calcMemberHash(index, range[index]);
        });
        range[index] = member;
        cachedHashTotal.applyIfValid([&](auto& value) {
            value += this->calcMemberHash(index, range[index]);
        });
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline lib::optional<HashType> notifyPossibleImageChange(UInt index) {
        debug_code(assert(index < rangeSize()));
        debug_code(standardSanityChecksForThisType());

        if (!cachedHashTotal.isValid()) {
            return lib::nullopt;
        }
        return calcMemberHash(index, getRange<InnerViewType>()[index]);
    }
    inline void checkNotUsingCachedHash() {
        if (cachedHashTotal.isValid()) {
            myCerr
                << "Error: constraint changing a member of a function without "
                   "passing a previous member hash.  Means no support for "
                   "getting total hash of this function.  Suspected "
                   "reason is that one of the constraints has not been "
                   "updated to support this.\n";
            myAbort();
        }
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline lib::optional<HashType> imageChanged(
        UInt index, lib::optional<HashType> previousMemberHash = lib::nullopt) {
        if (!previousMemberHash) {
            checkNotUsingCachedHash();
        }
        lib::optional<HashType> newHashOption;
        cachedHashTotal.applyIfValid([&](auto& value) {
            newHashOption = this->calcMemberHash<InnerViewType>(
                index, getRange<InnerViewType>()[index]);
            value -= *previousMemberHash;
            value += *newHashOption;
        });
        return newHashOption;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    HashType calcCombinedMembersHash(const std::vector<UInt>& indices) {
        HashType newHash(0);
        auto& range = getRange<InnerViewType>();
        for (UInt index : indices) {
            debug_code(assert(index < range.size()));
            newHash += calcMemberHash(index, range[index]);
        }
        return newHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline lib::optional<HashType> notifyPossibleImagesChange(
        const std::vector<UInt>& indices) {
        debug_code(standardSanityChecksForThisType());
        if (!cachedHashTotal.isValid()) {
            return lib::nullopt;
        }
        return calcCombinedMembersHash<InnerViewType>(indices);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline lib::optional<HashType> imagesChanged(
        const std::vector<UInt>& indices,
        lib::optional<HashType> previousMembersCombinedHash) {
        if (!previousMembersCombinedHash) {
            checkNotUsingCachedHash();
        }
        lib::optional<HashType> newHashOption;
        cachedHashTotal.applyIfValid([&](auto& value) {
            newHashOption = calcCombinedMembersHash<InnerViewType>(indices);
            value -= *previousMembersCombinedHash;
            value += *newHashOption;
        });
        return newHashOption;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void swapImages(UInt index1, UInt index2) {
        auto& range = getRange<InnerViewType>();
        cachedHashTotal.applyIfValid([&](auto& value) {
            value -= this->calcMemberHash(index1, range[index1]);
            value -= this->calcMemberHash(index2, range[index2]);
        });

        std::swap(range[index1], range[index2]);
        cachedHashTotal.applyIfValid([&](auto& value) {
            value += this->calcMemberHash(index1, range[index1]);
            value += this->calcMemberHash(index2, range[index2]);
        });

        debug_code(standardSanityChecksForThisType());
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
                cachedHashTotal.invalidate();
                membersImpl.clear();
            },
            range);
    }
};

#endif /* SRC_TYPES_FUNCTION_H_ */
