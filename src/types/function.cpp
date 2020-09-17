#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>
#include "common/common.h"
#include "types/boolVal.h"
#include "types/enumVal.h"
#include "types/functionVal.h"
#include "types/intVal.h"
#include "types/tupleVal.h"
#include "utils/ignoreUnused.h"
#include "utils/safePow.h"
using namespace std;
template <>
void deepCopy<FunctionValue>(const FunctionValue& src, FunctionValue& target);
struct NoSupportException {};
Dimension intDomainToDimension(IntDomain& dom) {
    if (dom.bounds.size() > 1) {
        myCerr
            << "Error: this function does not support int domains with holes "
               "in it.\n";
        throw NoSupportException();
    }
    return Dimension(dom.bounds.front().first, dom.bounds.front().second);
}

bool canBuildDimensionVec(const AnyDomainRef& domain) {
    return lib::visit(
        overloaded([&](const shared_ptr<IntDomain>&) { return true; },
                   [&](const shared_ptr<EnumDomain>&) { return true; },
                   [&](const shared_ptr<BoolDomain>&) { return true; },
                   [&](const shared_ptr<TupleDomain>& domain) {
                       for (const auto& d : domain->inners) {
                           if (!canBuildDimensionVec(d)) {
                               return false;
                           }
                       }
                       return true;
                   },
                   [&](const auto&) { return false; }),
        domain);
}

void makeDimensionVecFromDomainHelper(const AnyDomainRef& domain,
                                      DimensionVec& dimVec) {
    try {
        lib::visit(overloaded(
                       [&](const shared_ptr<IntDomain>& domain) {
                           dimVec.emplace_back(intDomainToDimension(*domain));
                       },
                       [&](const shared_ptr<EnumDomain>& domain) {
                           dimVec.emplace_back(0, domain->numberValues() - 1);
                       },
                       [&](const shared_ptr<TupleDomain>& domain) {
                           for (auto& inner : domain->inners) {
                               makeDimensionVecFromDomainHelper(inner, dimVec);
                           }
                       },
                       [&](const auto&) { throw NoSupportException(); }),
                   domain);

    } catch (...) {
        myCerr
            << "Currently no support for building DimensionVecs from function "
               "domain: ";
        prettyPrint(myCerr, domain) << endl;
        myAbort();
    }
}

DimensionVec makeDimensionVecFromDomain(const AnyDomainRef& domain) {
    DimensionVec dimensions;
    makeDimensionVecFromDomainHelper(domain, dimensions);
    size_t requiredSize = 1;
    for (auto iter = dimensions.rbegin(); iter != dimensions.rend(); ++iter) {
        auto& dim = *iter;
        dim.blockSize = requiredSize;
        requiredSize *= (dim.upper - dim.lower) + 1;
    }
    return dimensions;
}

template <typename Op>
struct OpMaker;

struct OpTupleLit;

template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(std::vector<AnyExprRef> members);
};

lib::optional<UInt> translateValueFromDimension(Int value,
                                                const Dimension& dimension) {
    if (value >= dimension.lower && value <= dimension.upper) {
        return value - dimension.lower;
    }
    return lib::nullopt;
}
template <typename View>
lib::optional<UInt> preimageToIndexHelper(const View&, const DimensionVec&,
                                          size_t&) {
    todoImpl();
}

lib::optional<UInt> preimageToIndexHelper(const EnumView& v,
                                          const DimensionVec& dimVec,
                                          size_t& dimIndex) {
    debug_code(assert(dimIndex < dimVec.size()));
    auto i = translateValueFromDimension(v.value, dimVec[dimIndex]);
    ++dimIndex;
    return i;
}

lib::optional<UInt> preimageToIndexHelper(const IntView& v,
                                          const DimensionVec& dimVec,
                                          size_t& dimIndex) {
    debug_code(assert(dimIndex < dimVec.size()));
    auto i = translateValueFromDimension(v.value, dimVec[dimIndex]);
    ++dimIndex;
    return i;
}
lib::optional<UInt> preimageToIndexHelper(const TupleView& tupleV,
                                          const DimensionVec& dimVec,
                                          size_t& dimIndex) {
    debug_code(assert(dimIndex < dimVec.size()));
    size_t indexTotal = 0;
    bool undefined = false;
    for (auto& member : tupleV.members) {
        size_t origDimIndex = dimIndex;
        lib::visit(
            [&](const auto& expr) {
                const auto memberView = expr->getViewIfDefined();
                if (!memberView) {
                    undefined = true;
                    return;
                }
                auto index =
                    preimageToIndexHelper(*memberView, dimVec, dimIndex);
                if (!index) {
                    undefined = true;
                    return;
                }
                indexTotal += dimVec[origDimIndex].blockSize * (*index);
            },
            member);
        if (undefined) {
            return lib::nullopt;
        }
    }
    return indexTotal;
}

lib::optional<UInt> FunctionView::preimageToIndex(const IntView& v) {
    if (!lazyPreimages()) {
        return nonLazyDomainToIndex(v);
    }
    size_t index = 0;
    return preimageToIndexHelper(v, getDimensions(), index);
}

lib::optional<UInt> FunctionView::preimageToIndex(const EnumView& v) {
    if (!lazyPreimages()) {
        return nonLazyDomainToIndex(v);
    }

    size_t index = 0;
    return preimageToIndexHelper(v, getDimensions(), index);
}

lib::optional<UInt> FunctionView::preimageToIndex(const TupleView& v) {
    if (!lazyPreimages()) {
        return nonLazyDomainToIndex(v);
    }

    size_t index = 0;
    return preimageToIndexHelper(v, getDimensions(), index);
}

template <>
ExprRef<IntView> functionIndexToPreimage<IntDomain>(
    const IntDomain&, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex) {
    debug_code(assert(dimIndex < dimensions.size()));
    auto val = make<IntValue>();
    val->value = dimensions[dimIndex].lower + index;
    ++dimIndex;
    return val.asExpr();
}

template <>
void functionIndexToPreimage<IntDomain>(const IntDomain&,
                                        const DimensionVec& dimensions,
                                        UInt index, IntView& view,
                                        size_t& dimIndex) {
    debug_code(assert(dimIndex < dimensions.size()));
    view.changeValue([&]() {
        view.value = dimensions[dimIndex].lower + index;
        return true;
    });
    ++dimIndex;
}

template <>
ExprRef<EnumView> functionIndexToPreimage<EnumDomain>(
    const EnumDomain&, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex) {
    debug_code(assert(dimIndex < dimensions.size()));
    auto val = make<EnumValue>();
    val->value = dimensions[dimIndex].lower + index;
    ++dimIndex;
    return val.asExpr();
}

template <>
void functionIndexToPreimage<EnumDomain>(const EnumDomain&,
                                         const DimensionVec& dimensions,
                                         UInt index, EnumView& view,
                                         size_t& dimIndex) {
    debug_code(assert(dimIndex < dimensions.size()));
    view.changeValue([&]() {
        view.value = dimensions[dimIndex].lower + index;
        return true;
    });
    ++dimIndex;
}

template <>
ExprRef<TupleView> functionIndexToPreimage<TupleDomain>(
    const TupleDomain& domain, const DimensionVec& dimVec, UInt index,
    size_t& dimIndex) {
    vector<AnyExprRef> tupleMembers;

    for (auto& inner : domain.inners) {
        UInt row = index / dimVec[dimIndex].blockSize;
        index %= dimVec[dimIndex].blockSize;
        lib::visit(
            [&](auto& innerDomainPtr) {
                tupleMembers.emplace_back(functionIndexToPreimage(
                    *innerDomainPtr, dimVec, row, dimIndex));
            },
            inner);
    }
    auto tuple = OpMaker<OpTupleLit>::make(move(tupleMembers));
    tuple->setAppearsDefined(true);
    tuple->setEvaluated(true);
    return tuple;
}

template <>
void functionIndexToPreimage<TupleDomain>(const TupleDomain& domain,
                                          const DimensionVec& dimensions,
                                          UInt index, TupleView& view,
                                          size_t& dimIndex) {
    debug_code(assert(view.members.size() == domain.inners.size()));
    for (size_t i = 0; i < domain.inners.size(); i++) {
        auto& dim = dimensions[dimIndex];
        lib::visit(
            [&](auto& innerDomainPtr) {
                typedef
                    typename BaseType<decltype(innerDomainPtr)>::element_type
                        Domain;
                typedef typename AssociatedValueType<Domain>::type Value;
                typedef typename AssociatedViewType<Value>::type View;
                auto& memberExpr = lib::get<ExprRef<View>>(view.members[i]);
                auto& memberView = memberExpr->view().get();
                UInt row = index / dim.blockSize;
                index %= dim.blockSize;
                functionIndexToPreimage(*innerDomainPtr, dimensions, row,
                                        memberView, dimIndex);
            },
            domain.inners[i]);
    }
}

template <>
HashType getValueHash<FunctionView>(const FunctionView& val) {
    return val.cachedHashes
        .getOrSet([&]() {
            return lib::visit(
                [&](const auto& range) {
                    CachedHashes cachedHashes;
                    cachedHashes.rangeHashes.resize(
                        range.size(), HashType(0));  // 0 will be replaced

                    for (size_t i = 0; i < range.size(); ++i) {
                        cachedHashes.assignRangeHash(i, getValueHash(range[i]),
                                                     false);
                        cachedHashes.hashTotal +=
                            val.calcMemberHashFromCache(i, cachedHashes);
                    }
                    return cachedHashes;
                },
                val.range);
        })
        .hashTotal;
}

template <>
ostream& prettyPrint<FunctionView>(ostream& os, const FunctionView& v) {
    os << "function(";
    lib::visit(
        [&](auto& rangeImpl) {
            for (size_t i = 0; i < rangeImpl.size(); i++) {
                if (i > 0) {
                    os << ",\n";
                }
                lib::visit(
                    [&](auto& preimageDomain) {
                        typedef typename BaseType<decltype(
                            preimageDomain)>::element_type Domain;
                        auto from = v.indexToPreimage<Domain>(i);
                        prettyPrint(os, from->view());
                    },
                    v.preimageDomain);
                os << " --> ";
                prettyPrint(os, rangeImpl[i]->view());
            }
        },
        v.range);
    os << ")";
    return os;
}

template <>
ostream& prettyPrint<FunctionView>(ostream& os, const FunctionDomain& domain,
                                   const FunctionView& v) {
    os << ((domain.isMatrixDomain) ? "[" : "function(");
    lib::visit(
        [&](auto& rangeImpl) {
            typedef
                typename AssociatedValueType<viewType(rangeImpl)>::type Value;
            typedef typename AssociatedDomain<Value>::type Domain;
            const auto& toDomainPtr = lib::get<shared_ptr<Domain>>(domain.to);

            for (size_t i = 0; i < rangeImpl.size(); i++) {
                if (i > 0) {
                    os << ",";
                }
                if (!domain.isMatrixDomain) {
                    lib::visit(
                        [&](auto& preimageDomain) {
                            typedef typename BaseType<decltype(
                                preimageDomain)>::element_type Domain;
                            auto from = v.indexToPreimage<Domain>(i);
                            prettyPrint(os, *preimageDomain, from->view());
                        },
                        v.preimageDomain);

                    os << " --> ";
                }
                prettyPrint(os, *toDomainPtr, rangeImpl[i]->view());
            }
        },
        v.range);
    if (domain.isMatrixDomain) {
        os << "; ";
        prettyPrint(os, domain.from) << "]";
    } else {
        os << ")";
    }
    return os;
}

AnyExprVec deepCopyRange(const AnyExprVec& range) {
    return lib::visit(
        [&](auto& range) -> AnyExprVec {
            ExprRefVec<viewType(range)> newRange;
            for (auto& member : range) {
                newRange.emplace_back(
                    deepCopy(*assumeAsValue(member)).asExpr());
            }
            return newRange;
        },
        range);
}

Preimages deepCopyPreimages(const Preimages& preimages) {
    if (lib::get_if<DimensionVec>(&preimages)) {
        return preimages;
    } else {
        const auto& preimageContainer =
            lib::get<ExplicitPreimageContainer>(preimages);
        ExplicitPreimageContainer newPreimages = preimageContainer;
        // shallow copy first as this takes care of maps and vectors, now
        // replace exprs with deep copies
        lib::visit(
            [&](auto& newPreimageMembers) {
                for (auto& member : newPreimageMembers) {
                    member = deepCopy(*assumeAsValue(member)).asExpr();
                }
            },
            newPreimages.preimages);
        return newPreimages;
    }
}

template <>
void deepCopy<FunctionValue>(const FunctionValue& src, FunctionValue& target) {
    target.silentClear();
    target.initVal(src.preimageDomain, deepCopyPreimages(src.preimages),
                   deepCopyRange(src.range), src.partial);
    target.notifyEntireValueChanged();
}

template <>
ostream& prettyPrint<FunctionDomain>(ostream& os, const FunctionDomain& d) {
    if (d.isMatrixDomain) {
        os << "matrix indexed by [";
        prettyPrint(os, d.from) << "] of ";
        prettyPrint(os, d.to);
    } else {
        os << "function(";
        prettyPrint(os, d.from) << " --> ";
        prettyPrint(os, d.to) << ")";
    }
    return os;
}

void matchInnerType(const FunctionValue& src, FunctionValue& target) {
    lib::visit(
        [&](auto& srcRange) {
            target.range.emplace<ExprRefVec<viewType(srcRange)>>();
        },
        src.range);
    if (src.lazyPreimages()) {
        target.preimages = src.preimages;
        // copy over dimension  vec that is
    } else {
        lib::visit(
            [&](auto& preimages) {
                auto& targetPreimageContainer =
                    target.preimages.emplace<ExplicitPreimageContainer>();
                targetPreimageContainer.preimages
                    .emplace<ExprRefVec<viewType(preimages)>>();
            },
            src.getPreimages().preimages);
    }
}

void matchInnerType(const FunctionDomain& domain, FunctionValue& target) {
    lib::visit(
        [&](auto& rangeDomain) {
            typedef typename AssociatedViewType<typename BaseType<decltype(
                rangeDomain)>::element_type>::type RangeView;
            target.range.emplace<ExprRefVec<RangeView>>();
        },
        domain.to);
    if (canBuildDimensionVec(domain.from)) {
        target.preimages.emplace<DimensionVec>() =
            makeDimensionVecFromDomain(domain.from);
    } else {
        lib::visit(
            [&](auto& preimageDomain) {
                typedef typename AssociatedViewType<typename BaseType<decltype(
                    preimageDomain)>::element_type>::type PreimageView;
                target.preimages.emplace<ExplicitPreimageContainer>()
                    .preimages.emplace<ExprRefVec<PreimageView>>();
            },
            domain.from);
    }
}

template <>
UInt getDomainSize<FunctionDomain>(const FunctionDomain& domain) {
    assert(domain.partial == PartialAttr::TOTAL &&
           domain.jectivity == JectivityAttr::NONE);
    auto crossProd = safePow<UInt>(getDomainSize(domain.to),
                                   getDomainSize(domain.from), MAX_DOMAIN_SIZE);
    if (!crossProd) {
        return MAX_DOMAIN_SIZE;
    }
    return *crossProd;
}

void evaluateImpl(FunctionValue&) {}
void startTriggeringImpl(FunctionValue&) {}
void stopTriggering(FunctionValue&) {}

template <typename InnerViewType>
void normaliseImpl(FunctionValue&, ExprRefVec<InnerViewType>& valMembersImpl) {
    for (auto& v : valMembersImpl) {
        normalise(*assumeAsValue(v));
    }
}

template <>
void normalise<FunctionValue>(FunctionValue& val) {
    lib::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.range);
}

template <>
bool smallerValue<FunctionView>(const FunctionView& u, const FunctionView& v);
template <>
bool largerValue<FunctionView>(const FunctionView& u, const FunctionView& v);

template <>
bool smallerValue<FunctionView>(const FunctionView& u, const FunctionView& v) {
    return lib::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                lib::get<BaseType<decltype(uMembersImpl)>>(v.range);
            if (uMembersImpl.size() < vMembersImpl.size()) {
                return true;
            } else if (uMembersImpl.size() > vMembersImpl.size()) {
                return false;
            }
            for (size_t i = 0; i < uMembersImpl.size(); ++i) {
                if (smallerValue(uMembersImpl[i]->view(),
                                 vMembersImpl[i]->view())) {
                    return true;
                } else if (largerValue(uMembersImpl[i]->view(),
                                       vMembersImpl[i]->view())) {
                    return false;
                }
            }
            return false;
        },
        u.range);
}

template <>
bool largerValue<FunctionView>(const FunctionView& u, const FunctionView& v) {
    return lib::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                lib::get<BaseType<decltype(uMembersImpl)>>(v.range);
            if (uMembersImpl.size() > vMembersImpl.size()) {
                return true;
            } else if (uMembersImpl.size() < vMembersImpl.size()) {
                return false;
            }
            for (size_t i = 0; i < uMembersImpl.size(); ++i) {
                if (largerValue(uMembersImpl[i]->view(),
                                vMembersImpl[i]->view())) {
                    return true;
                } else if (smallerValue(uMembersImpl[i]->view(),
                                        vMembersImpl[i]->view())) {
                    return false;
                }
            }
            return false;
        },
        u.range);
}

void checkPreimages(const FunctionView& view) {
    if (view.lazyPreimages()) {
        sanityEqualsCheck(makeDimensionVecFromDomain(view.preimageDomain),
                          view.getDimensions());
        return;
    } else {
        auto preimages = view.getPreimages();
        lib::visit(
            [&](auto& members) {
                sanityEqualsCheck(view.rangeSize(), members.size());
                sanityEqualsCheck(members.size(),
                                  preimages.preimageHashes.size());
                sanityEqualsCheck(members.size(),
                                  preimages.preimageHashIndexMap.size());
                for (size_t i = 0; i < members.size(); i++) {
                    members[i]->debugSanityCheck();
                    sanityEqualsCheck(getValueHash(members[i]),
                                      preimages.preimageHashes[i]);
                    sanityCheck(preimages.preimageHashIndexMap.count(
                                    preimages.preimageHashes[i]),
                                "hash missing from preimage hash index map");
                    sanityEqualsCheck(
                        i,
                        preimages
                            .preimageHashIndexMap[preimages.preimageHashes[i]]);
                }
            },
            preimages.preimages);
    }
}
void FunctionView::standardSanityChecksForThisType() const {
    checkPreimages(*this);

    lib::visit(
        [&](auto& range) {
            UInt checkNumberUndefined = 0;
            HashType checkCachedHashTotal(0);
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                sanityEqualsCheck(range.size(),
                                  cachedHashes.rangeHashes.size());
            });
            for (size_t i = 0; i < range.size(); i++) {
                if (!range[i]->appearsDefined()) {
                    checkNumberUndefined++;
                    continue;
                }
                cachedHashes.applyIfValid([&](auto& cachedHashes) {
                    auto hash = getValueHash(range[i]);
                    sanityEqualsCheck(hash, cachedHashes.rangeHashes[i]);
                    checkCachedHashTotal +=
                        calcMemberHashFromCache(i, cachedHashes);
                });
            }
            sanityEqualsCheck(checkNumberUndefined, numberUndefined);
            cachedHashes.applyIfValid([&](auto& cachedHashes) {
                sanityEqualsCheck(checkCachedHashTotal, cachedHashes.hashTotal);
            });
        },
        this->range);
}

void FunctionValue::debugSanityCheckImpl() const {
    this->standardSanityChecksForThisType();
    lib::visit(
        [&](const auto& valMembersImpl) {
            // check var bases
            varBaseSanityChecks(this->imageValBase, valMembersImpl);
        },
        range);
    if (!lazyPreimages()) {
        lib::visit(
            [&](const auto& valMembersImpl) {
                // check var bases
                varBaseSanityChecks(this->preimageValBase, valMembersImpl);
            },
            getPreimages().preimages);
    }
}

AnyExprVec& FunctionValue::getChildrenOperands() { return range; }

template <>
size_t getResourceLowerBound<FunctionDomain>(const FunctionDomain& domain) {
    return domain.sizeAttr.minSize * getResourceLowerBound(domain.from) +
           domain.sizeAttr.minSize * getResourceLowerBound(domain.to) + 1;
}
