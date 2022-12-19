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
    SizeAttr sizeAttr = noSize();
    bool isMatrixDomain = false;
    template <typename FromDomainType, typename ToDomainType>
    FunctionDomain(JectivityAttr jectivity, PartialAttr partial,
                   FromDomainType&& from, ToDomainType&& to)
        : jectivity(jectivity),
          partial(partial),
          from(makeAnyDomainRef(std::forward<FromDomainType>(from))),
          to(makeAnyDomainRef(std::forward<ToDomainType>(to))) {
        if (this->partial == PartialAttr::TOTAL) {
            this->sizeAttr = exactSize(getDomainSize(this->from));
        }
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
    }
};

struct FunctionValue : public FunctionView, public ValBase {
    // function members valbases do not point directly to the function, they
    // point to an image or pre image container.  The class below simply derives
    // ValBase so that other parts of the code can detect via dynamic cast that
    // the parent ofthe function members is one of these function Objects.
    struct FunctionImageContainer : public ValBase {
        using ValBase::ValBase;
        inline bool supportsDefinedVars() final { return true; }
        inline void notifyVarDefined(UInt memberId) final {
            auto& val = dynamic_cast<FunctionValue&>(*this->container);
            lib::visit(
                [&](auto& members) {
                    val.imageChanged<viewType(members)>(memberId);
                    val.notifyImageChanged(memberId);
                },
                val.range);
        }
    };
    struct FunctionPreimageContainer : public ValBase {
        using ValBase::ValBase;
        inline bool supportsDefinedVars() final { return false; }
    };

    static const UInt PREIMAGE_VALBASE_INDEX = 0;
    static const UInt IMAGE_VALBASE_INDEX = 1;
    FunctionPreimageContainer preimageValBase;
    FunctionImageContainer imageValBase;
    FunctionValue()
        : preimageValBase(PREIMAGE_VALBASE_INDEX, this),
          imageValBase(IMAGE_VALBASE_INDEX, this) {}

    inline bool supportsDefinedVars() final { return true; }
    inline void notifyVarDefined(UInt) final {}
    void initView(AnyDomainRef, Preimages, AnyExprVec, bool) {
        myCerr << "Call init val instead\n";
        shouldNotBeCalledPanic;
    }

    void initVal(AnyDomainRef preImageDomain, Preimages preimages,
                 AnyExprVec range, bool partial) {
        FunctionView::initView(preImageDomain, std::move(preimages),
                               std::move(range), partial);
        lib::visit(
            [&](auto& range) {
                for (size_t i = 0; i < range.size(); i++) {
                    auto& val = *assumeAsValue(range[i]);
                    valBase(val).container = &imageValBase;
                    valBase(val).id = i;
                }
            },
            this->range);
        if (!lazyPreimages()) {
            lib::visit(
                [&](auto& preimages) {
                    for (size_t i = 0; i < preimages.size(); i++) {
                        auto& val = *assumeAsValue(preimages[i]);
                        valBase(val).container = &preimageValBase;
                        valBase(val).id = i;
                    }
                },
                getPreimages().preimages);
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> getImageVal(UInt index) {
        return assumeAsValue(
            getRange<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }
    // only works for functions using explicit pre images
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> getPreimageVal(UInt index) {
        return assumeAsValue(getPreimages()
                                 .template get<typename AssociatedViewType<
                                     InnerValueType>::type>()[index]);
    }

    AnyExprVec& getChildrenOperands() final;

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
    inline bool tryImageChange(UInt index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        lib::optional<HashType> previousImageHash =
            cachedHashes.calcIfValid([&](auto& cachedHashes) {
                return cachedHashes.rangeHashes[index];
            });
        FunctionView::imageChanged<InnerViewType>(index);
        if (func()) {
            FunctionView::notifyImageChanged(index);
            return true;
        } else {
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                cachedHashes.hashTotal -=
                    calcMemberHashFromCache(index, cachedHashes);
                cachedHashes.assignRangeHash(index, previousImageHash.value());
                cachedHashes.hashTotal +=
                    calcMemberHashFromCache(index, cachedHashes);
            });
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryImagesChange(const std::vector<UInt>& indices, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        lib::optional<std::vector<HashType>> previousImageHashes =
            cachedHashes.calcIfValid([&](auto& cachedHashes) {
                std::vector<HashType> hashes;
                for (auto index : indices) {
                    hashes.emplace_back(cachedHashes.rangeHashes[index]);
                    ;
                }
                return hashes;
            });
        FunctionView::imagesChanged<InnerViewType>(indices);
        if (func()) {
            FunctionView::notifyImagesChanged(indices);
            return true;
        } else {
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                const auto& previousHashes = previousImageHashes.value();
                for (size_t i = 0; i < indices.size(); i++) {
                    auto index = indices[i];
                    auto hash = previousHashes[i];
                    cachedHashes.hashTotal -=
                        calcMemberHashFromCache(index, cachedHashes);
                    cachedHashes.assignRangeHash(index, hash);
                    cachedHashes.hashTotal +=
                        calcMemberHashFromCache(index, cachedHashes);
                }
            });
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryPreimageChange(UInt index, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        debug_code(assert(index < rangeSize()));
        HashType previousPreimageHash = getPreimages().preimageHashes[index];

        FunctionView::preimageChanged<InnerViewType>(index);
        if (func()) {
            FunctionView::notifyPreimageChanged(index, previousPreimageHash);
            return true;
        } else {
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                cachedHashes.hashTotal -=
                    calcMemberHashFromCache(index, cachedHashes);
            });
            getPreimages().change(index, previousPreimageHash);
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                cachedHashes.hashTotal +=
                    calcMemberHashFromCache(index, cachedHashes);
            });
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryPreimagesChange(const std::vector<UInt>& indices,
                                   Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        std::vector<HashType> previousPreimageHashes;
        for (auto index : indices) {
            previousPreimageHashes.emplace_back(
                getPreimages().preimageHashes[index]);
        }

        FunctionView::preimagesChanged<InnerViewType>(indices);
        if (func()) {
            FunctionView::notifyPreimagesChanged(indices,
                                                 previousPreimageHashes);
            return true;
        } else {
            for (size_t i = 0; i < indices.size(); i++) {
                auto index = indices[i];
                auto hash = previousPreimageHashes[i];
                cachedHashes.applyIfValid([&](auto& cachedHashes) {
                    cachedHashes.hashTotal -=
                        calcMemberHashFromCache(index, cachedHashes);
                });
                getPreimages().change(index, hash);
                cachedHashes.applyIfValid([&](auto& cachedHashes) {
                    cachedHashes.hashTotal +=
                        calcMemberHashFromCache(index, cachedHashes);
                });
            }
            return false;
        }
    }

    template <typename PreimageValueType, typename ImageValueType,
              typename Func, EnableIfValue<PreimageValueType> = 0,
              EnableIfValue<ImageValueType> = 0>
    inline bool tryAddValue(const ValRef<PreimageValueType>& preimage,
                            const ValRef<ImageValueType>& image, Func&& func) {
        if (FunctionView::addValue(preimage.asExpr(), image.asExpr())) {
            if (func()) {
                valBase(*preimage).container = &preimageValBase;
                valBase(*image).container = &imageValBase;
                valBase(*preimage).id = rangeSize() - 1;
                valBase(*image).id = rangeSize() - 1;
                FunctionView::notifyValueAdded(preimage.asExpr(),
                                               image.asExpr());
                return true;
            } else {
                typedef typename AssociatedViewType<PreimageValueType>::type
                    PreimageViewType;
                typedef typename AssociatedViewType<ImageValueType>::type
                    ImageViewType;
                FunctionView::removeValue<PreimageViewType, ImageViewType>(
                    rangeSize() - 1);
            }
        }
        return false;
    }

    template <typename PreimageValueType, typename ImageValueType,
              typename Func, EnableIfValue<PreimageValueType> = 0,
              EnableIfValue<ImageValueType> = 0>
    inline lib::optional<
        std::pair<ValRef<PreimageValueType>, ValRef<ImageValueType>>>
    tryRemoveValue(UInt index, Func&& func) {
        typedef typename AssociatedViewType<PreimageValueType>::type
            PreimageViewType;
        typedef typename AssociatedViewType<ImageValueType>::type ImageViewType;
        debug_code(assert(index < rangeSize()));
        auto preimage = getPreimageVal<PreimageValueType>(index);
        auto image = getImageVal<ImageValueType>(index);
        FunctionView::removeValue<PreimageViewType, ImageViewType>(index);
        if (func()) {
            valBase(*preimage).container = NULL;
            valBase(*image).container = NULL;
            if (index < rangeSize()) {
                valBase(*getPreimageVal<PreimageValueType>(index)).id = index;
                valBase(*getImageVal<ImageValueType>(index)).id = index;
            }
            FunctionView::notifyValueRemoved(index, preimage.asExpr(),
                                             image.asExpr());
            return std::make_pair(preimage, image);
        } else {
            FunctionView::addValue(preimage.asExpr(), image.asExpr());
            swapValues<PreimageViewType, ImageViewType>(index, rangeSize() - 1);
            debug_code(standardSanityChecksForThisType());
            return lib::nullopt;
        }
    }

    // helper for tryRemoveValue
   private:
    template <typename PreimageViewType, typename ImageViewType,
              EnableIfView<PreimageViewType> = 0,
              EnableIfView<ImageViewType> = 0>
    inline void swapValues(UInt index1, UInt index2) {
        auto& range = getRange<ImageViewType>();
        std::swap(range[index1], range[index2]);
        cachedHashes.applyIfValid([&](auto& cachedHashes) {
            cachedHashes.swapRangeHashes(index1, index2);
        });
        auto& preimages = getPreimages();
        preimages.swap<PreimageViewType>(index1, index2);
    }

   public:
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
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    bool isUndefined();
    std::pair<bool, ExprRef<FunctionView>> optimiseImpl(ExprRef<FunctionView>&,
                                                        PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
    void hashChecksImpl() const final;
};

#endif /* SRC_TYPES_FUNCTIONVAL_H_ */
