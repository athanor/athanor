#ifndef SRC_TYPES_FUNCTIONVAL_H_
#define SRC_TYPES_FUNCTIONVAL_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/function.h"
#include "types/intVal.h"
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
    bool isMatrixDomain = false;
    template <typename FromDomainType, typename ToDomainType>
    FunctionDomain(JectivityAttr jectivity, PartialAttr partial,
                   FromDomainType&& from, ToDomainType&& to)
        : jectivity(jectivity),
          partial(partial),
          from(makeAnyDomainRef(std::forward<FromDomainType>(from))),
          to(makeAnyDomainRef(std::forward<ToDomainType>(to))) {
        checkSupported();
    }

    template <typename DomainType>
    static std::shared_ptr<FunctionDomain> makeMatrixDomain(
        std::shared_ptr<IntDomain> indexingDomain, DomainType&& inner) {
        if (indexingDomain->bounds.size() > 1) {
            myCerr
                << "Error: currently no support for matrix domains with holes "
                   "in the index.\n";
            myAbort();
        }
        auto matrix = std::make_shared<FunctionDomain>(
            JectivityAttr::NONE, PartialAttr::TOTAL, indexingDomain,
            std::forward<DomainType>(inner));
        matrix->isMatrixDomain = true;
        return matrix;
    }

    inline std::shared_ptr<FunctionDomain> deepCopy(
        std::shared_ptr<FunctionDomain>&) {
        auto newDomain = std::make_shared<FunctionDomain>(*this);
        newDomain->from = deepCopyDomain(from);
        newDomain->to = deepCopyDomain(to);
        return newDomain;
    }

    void merge(FunctionDomain& other) {
        mergeDomains(from, other.from);
        mergeDomains(to, other.to);
    }

   private:
    void checkSupported() {
        if (jectivity != JectivityAttr::NONE) {
            myCerr
                << "Sorry, jectivity for functions must currently be None.\n";
            myAbort();
        }
        if (partial != PartialAttr::TOTAL) {
            myCerr << "Error, functions must currently be total.\n";
            myAbort();
        }
    }
};

struct FunctionValue : public FunctionView, public ValBase {
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getRange<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }
    AnyExprVec& getChildrenOperands() final;
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
            debug_code(debugSanityCheck());
            return true;
        } else {
            FunctionView::swapImages<InnerViewType>(index1, index2);
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryImageChange(UInt index,
                               lib::optional<HashType> previousMemberHash,
                               Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto newMemberHash = FunctionView::imageChanged<InnerViewType>(
            index, previousMemberHash);
        if (func()) {
            FunctionView::notifyImageChanged(index);
            return true;
        } else {
            cachedHashTotal.applyIfValid([&](auto& value) {
                debug_code(assert(previousMemberHash && newMemberHash));
                value -= *previousMemberHash;
                value += *newMemberHash;
            });
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryImagesChange(
        const std::vector<UInt>& indices,
        lib::optional<HashType> previousMembersCombinedHash, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto newMembersCombinedHash =
            FunctionView::imagesChanged<InnerViewType>(
                indices, previousMembersCombinedHash);
        if (func()) {
            FunctionView::notifyImagesChanged(indices);
            return true;
        } else {
            cachedHashTotal.applyIfValid([&](auto& value) {
                debug_code(assert(previousMembersCombinedHash &&
                                  newMembersCombinedHash));
                value -= *previousMembersCombinedHash;
                value += *newMembersCombinedHash;
            });
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
    std::pair<bool, ExprRef<FunctionView>> optimiseImpl(ExprRef<FunctionView>&,
                                                        PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_FUNCTIONVAL_H_ */
