#ifndef SRC_TYPES_FUNCTION_H_
#define SRC_TYPES_FUNCTION_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "optional.hpp"
#include "triggers/functionTrigger.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

struct Dimension {
    // dimensions are like int domain bounds, lower and upper inclusive
    Int lower;
    Int upper;
    UInt blockSize = 0;
    Dimension(Int lower, Int upper) : lower(lower), upper(upper) {}
};
typedef std::vector<Dimension> DimensionVec;

template <typename View>
ExprRef<View> functionIndexToDomain(const DimensionVec&, UInt) {
    static_assert(!std::is_same<View, IntView>::value,
                  "Currently do not support function from this type.\n");
}

template <>
ExprRef<IntView> functionIndexToDomain<IntView>(const DimensionVec& dimensions,
                                                UInt index);

template <>
ExprRef<TupleView> functionIndexToDomain<TupleView>(
    const DimensionVec& dimensions, UInt);

template <typename View>
void functionIndexToDomain(const DimensionVec&, UInt, View&) {
    static_assert(!std::is_same<View, IntView>::value,
                  "Currently do not support function from this type.\n");
}

template <>
void functionIndexToDomain<IntView>(const DimensionVec& dimensions, UInt index,
                                    IntView&);

template <>
void functionIndexToDomain<TupleView>(const DimensionVec& dimensions, UInt,
                                      TupleView&);
namespace lib {
using std::experimental::make_optional;
using std::experimental::nullopt;
using std::experimental::optional;
}  // namespace lib
lib::optional<Int> getAsIntForFunctionIndex(const AnyExprRef& expr);

struct FunctionView : public ExprInterface<FunctionView>,
                      TriggerContainer<FunctionView> {
    friend FunctionValue;
    DimensionVec dimensions;
    AnyExprVec range;

    lib::optional<UInt> domainToIndex(const IntView& intV);
    lib::optional<UInt> domainToIndex(const TupleView& tupleV);

    template <typename View, EnableIfView<BaseType<View>> = 0>
    lib::optional<UInt> domainToIndex(const View&) {
        shouldNotBeCalledPanic;
    }

    template <typename View, EnableIfView<View> = 0>
    void indexToDomain(UInt index, View& view) const {
        functionIndexToDomain<View>(dimensions, index, view);
    }

    template <typename View, EnableIfView<View> = 0>
    ExprRef<View> indexToDomain(UInt index) const {
        return functionIndexToDomain<View>(dimensions, index);
    }

    static DimensionVec makeDimensionVecFromDomain(const AnyDomainRef& domain);

    template <typename InnerViewType, typename DimVec,
              EnableIfView<InnerViewType> = 0>
    void resetDimensions(DimVec&& dim) {
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
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void assignImage(size_t index,
                            const ExprRef<InnerViewType>& member) {
        auto& range = getRange<InnerViewType>();
        debug_code(assert(index < range.size()));
        range[index] = member;
    }

    inline void notifyPossibleImageChange(UInt index) {
        ignoreUnused(index);
        debug_code(assertValidState());
        // later fill in this function for injective functions
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void imageChanged(UInt index) {
        ignoreUnused(index);
    }

    inline void notifyPossibleImagesChange(const std::vector<UInt>& indices) {
        ignoreUnused(indices);
        debug_code(assertValidState());
        // later fill in this function for injective functions
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void imagesChanged(const std::vector<UInt>& indices) {
        ignoreUnused(indices);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void swapImages(UInt index1, UInt index2) {
        auto& range = getRange<InnerViewType>();
        std::swap(range[index1], range[index2]);
        debug_code(assertValidState());
    }

    inline void notifyEntireFunctionChange() {
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getRange() {
        return mpark::get<ExprRefVec<InnerViewType>>(range);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getRange() const {
        return mpark::get<ExprRefVec<InnerViewType>>(range);
    }

    inline UInt rangeSize() const {
        return mpark::visit([](auto& range) { return range.size(); }, range);
    }

    void assertValidState();
    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_FUNCTION_H_ */
