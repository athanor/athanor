#ifndef SRC_TYPES_FUNCTION_H_
#define SRC_TYPES_FUNCTION_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"
enum class JectivityAttr { NONE, INJECTIVE, SURJECTIVE, BIJECTIVE };
enum class PartialAttr { PARTIAL, TOTAL };
struct FunctionDomain {
    JectivityAttr jectivity;
    PartialAttr partial;
    AnyDomainRef from;
    AnyDomainRef to;

    template <typename FromDomainType, typename ToDomainType>
    FunctionDomain(JectivityAttr jectivity, PartialAttr partial,
                   FromDomainType&& from, ToDomainType&& to)
        : jectivity(jectivity),
          partial(partial),
          from(makeAnyDomainRef(std::forward<FromDomainType>(from))),
          to(makeAnyDomainRef(std::forward<ToDomainType>(to))) {
        checkSupported();
    }

   private:
    void checkSupported() {
        if (jectivity != JectivityAttr::NONE) {
            std::cerr
                << "Sorry, jectivity for functions must currently be None.\n";
            abort();
        }
        if (partial != PartialAttr::TOTAL) {
            std::cerr << "Error, functions must currently be total.\n";
            abort();
        }
    }
};
struct FunctionView;

struct Dimension {
    // dimensions are like int domain bounds, lower and upper inclusive
    Int lower;
    Int upper;
    UInt blockSize = 0;
    Dimension(Int lower, Int upper) : lower(lower), upper(upper) {}
};
typedef std::vector<Dimension> DimensionVec;

struct FunctionTrigger : public virtual TriggerBase {};
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
    const DimensionVec& dimensions, UInt index);

struct FunctionView : public ExprInterface<FunctionView> {
    friend FunctionValue;
    DimensionVec dimensions;
    AnyExprVec range;
    std::vector<std::shared_ptr<FunctionTrigger>> triggers;

    std::pair<bool, UInt> domainToIndex(const IntView& intV);
    std::pair<bool, UInt> domainToIndex(const TupleView& tupleV);

    template <typename View, EnableIfView<BaseType<View>> = 0>
    std::pair<bool, UInt> domainToIndex(const View&) {
        shouldNotBeCalledPanic;
    }
    template <typename View, EnableIfView<View> = 0>
    ExprRef<View> indexToDomain(UInt index) const {
        return functionIndexToDomain<View>(dimensions, index);
    }

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

    FunctionView() {}

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
};

struct FunctionValue : public FunctionView, public ValBase {
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getRange<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void assignImage(UInt index, const ValRef<InnerValueType>& member) {
        FunctionView::assignImage(index, member.asExpr());
        valBase(*member).container = this;
        valBase(*member).id = id;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void setInnerType() {
        if (mpark::get_if<ExprRefVec<InnerViewType>>(&(range)) == NULL) {
            range.emplace<ExprRefVec<InnerViewType>>();
        }
    }

    void printVarBases();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer&) final;
    ExprRef<FunctionView> deepCopySelfForUnrollImpl(
        const ExprRef<FunctionView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    std::pair<bool, ExprRef<FunctionView>> optimise(PathExtension) final;
};

template <typename Child>
struct ChangeTriggerAdapter<FunctionTrigger, Child>
    : public ChangeTriggerAdapterBase<FunctionTrigger, Child> {};

#endif /* SRC_TYPES_FUNCTION_H_ */
