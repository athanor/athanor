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
    debug_code(bool posFunctionValueChangeCalled = false);

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
    template <typename Func>
    void visitMemberTriggers(Func&& func, UInt index) {
        visitTriggers(func, allMemberTriggers);
        if (index < singleMemberTriggers.size()) {
            visitTriggers(func, singleMemberTriggers[index]);
        }
    }

    template <typename Func>
    void visitMemberTriggers(Func&& func, const std::vector<UInt>& indices) {
        visitTriggers(func, allMemberTriggers);
        for (auto& index : indices) {
            if (index < singleMemberTriggers.size()) {
                visitTriggers(func, singleMemberTriggers[index]);
            }
        }
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

    inline void notifyImageChanged(UInt index) {
        debug_code(assertValidState());
        visitMemberTriggers([&](auto& t) { t->imageChanged(index); }, index);
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

    inline void notifyImagesChanged(const std::vector<UInt>& indices) {
        debug_code(assertValidState());
        visitMemberTriggers([&](auto& t) { t->imageChanged(indices); },
                            indices);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void swapImages(UInt index1, UInt index2) {
        auto& range = getRange<InnerViewType>();
        std::swap(range[index1], range[index2]);
        debug_code(assertValidState());
    }

    inline void notifyImagesSwapped(UInt index1, UInt index2) {
        debug_code(assert(posFunctionValueChangeCalled);
                   posFunctionValueChangeCalled = false);

        visitTriggers([&](auto& t) { t->imageSwap(index1, index2); },
                      allMemberTriggers);
        if (index1 < singleMemberTriggers.size()) {
            visitTriggers([&](auto& t) { t->imageSwap(index1, index2); },
                          singleMemberTriggers[index1]);
        }
        if (index2 < singleMemberTriggers.size()) {
            visitTriggers([&](auto& t) { t->imageSwap(index1, index2); },
                          singleMemberTriggers[index2]);
        }
    }

    inline void notifyEntireFunctionChange() {
        debug_code(assert(posFunctionValueChangeCalled);
                   posFunctionValueChangeCalled = false);
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
        valBase(*member).id = index;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void setInnerType() {
        if (mpark::get_if<ExprRefVec<InnerViewType>>(&(range)) == NULL) {
            range.emplace<ExprRefVec<InnerViewType>>();
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool trySwapImages(UInt index1, UInt index2, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        FunctionView::swapImages<InnerViewType>(index1, index2);
        if (func()) {
            auto& range = getRange<InnerViewType>();
            valBase(*assumeAsValue(range[index1])).id = index1;
            valBase(*assumeAsValue(range[index2])).id = index2;
            FunctionView::notifyImagesSwapped(index1, index2);
            debug_code(assertValidVarBases());
            return true;
        } else {
            FunctionView::swapImages<InnerViewType>(index1, index2);
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryImageChange(UInt index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        FunctionView::imageChanged<InnerViewType>(index);
        if (func()) {
            FunctionView::notifyImageChanged(index);
            return true;
        } else {
            FunctionView::imageChanged<InnerViewType>(index);
            return false;
        }
    }
    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryImagesChange(const std::vector<UInt>& indices, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        FunctionView::imagesChanged<InnerViewType>(indices);
        if (func()) {
            FunctionView::notifyImagesChanged(indices);
            return true;
        } else {
            FunctionView::imagesChanged<InnerViewType>(indices);
            return false;
        }
    }
    template <typename Func>
    bool tryAssignNewValue(FunctionValue& newvalue, Func&& func) {
        // fake putting in the value first untill func()verifies that it is
        // happy with the change
        std::swap(*this, newvalue);
        bool allowed = func();
        std::swap(*this, newvalue);
        if (allowed) {
            deepCopy(newvalue, *this);
        }
        return allowed;
    }

    void printVarBases();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<FunctionView> deepCopyForUnrollImpl(
        const ExprRef<FunctionView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    std::pair<bool, ExprRef<FunctionView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;

    void assertValidVarBases();
};

#endif /* SRC_TYPES_FUNCTION_H_ */
