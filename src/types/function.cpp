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
struct NoSupportException {};
Dimension intDomainToDimension(IntDomain& dom) {
    if (dom.bounds.size() > 1) {
        cerr << "Error: this function does not support int domains with holes "
                "in it.\n";
        throw NoSupportException();
    }
    return Dimension(dom.bounds.front().first, dom.bounds.front().second);
}

void makeDimensionVecFromDomainHelper(const AnyDomainRef& domain,
                                      DimensionVec& dimVec) {
    try {
        mpark::visit(
            overloaded(
                [&](const shared_ptr<IntDomain>& domain) {
                    dimVec.emplace_back(intDomainToDimension(*domain));
                },
                [&](const shared_ptr<EnumDomain>& domain) {
                    dimVec.emplace_back(0, domain->valueNames.size() - 1);
                },
                [&](const shared_ptr<TupleDomain>& domain) {
                    for (auto& inner : domain->inners) {
                        makeDimensionVecFromDomainHelper(inner, dimVec);
                    }
                },
                [&](const auto&) { throw NoSupportException(); }),
            domain);

    } catch (...) {
        cerr << "Currently no support for building DimensionVecs from function "
                "domain: ";
        prettyPrint(cerr, domain) << endl;
        abort();
    }
}

DimensionVec FunctionView::makeDimensionVecFromDomain(
    const AnyDomainRef& domain) {
    DimensionVec dimVec;
    makeDimensionVecFromDomainHelper(domain, dimVec);

    return dimVec;
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
lib::optional<UInt> domainToIndexHelper(const View&, const DimensionVec&,
                                        size_t&) {
    todoImpl();
}

lib::optional<UInt> domainToIndexHelper(const EnumView& v,
                                        const DimensionVec& dimVec,
                                        size_t& dimIndex) {
    debug_code(assert(dimIndex < dimVec.size()));
    auto i = translateValueFromDimension(v.value, dimVec[dimIndex]);
    ++dimIndex;
    return i;
}

lib::optional<UInt> domainToIndexHelper(const IntView& v,
                                        const DimensionVec& dimVec,
                                        size_t& dimIndex) {
    debug_code(assert(dimIndex < dimVec.size()));
    auto i = translateValueFromDimension(v.value, dimVec[dimIndex]);
    ++dimIndex;
    return i;
}
lib::optional<UInt> domainToIndexHelper(const TupleView& tupleV,
                                        const DimensionVec& dimVec,
                                        size_t& dimIndex) {
    debug_code(assert(dimIndex < dimVec.size()));
    size_t indexTotal = 0;
    bool undefined = false;
    for (auto& member : tupleV.members) {
        size_t origDimIndex = dimIndex;
        mpark::visit(
            [&](const auto& expr) {
                const auto memberView = expr->getViewIfDefined();
                if (!memberView) {
                    undefined = true;
                    return;
                }
                auto index = domainToIndexHelper(*memberView, dimVec, dimIndex);
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

lib::optional<UInt> FunctionView::domainToIndex(const IntView& v) {
    size_t index = 0;
    return domainToIndexHelper(v, dimensions, index);
}

lib::optional<UInt> FunctionView::domainToIndex(const EnumView& v) {
    size_t index = 0;
    return domainToIndexHelper(v, dimensions, index);
}

lib::optional<UInt> FunctionView::domainToIndex(const TupleView& v) {
    size_t index = 0;
    return domainToIndexHelper(v, dimensions, index);
}

template <>
ExprRef<IntView> functionIndexToDomain<IntDomain>(
    const IntDomain&, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex) {
    debug_code(assert(dimIndex < dimensions.size()));
    auto val = make<IntValue>();
    val->value = dimensions[dimIndex].lower + index;
    ++dimIndex;
    return val.asExpr();
}

template <>
void functionIndexToDomain<IntDomain>(const IntDomain&,
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
ExprRef<EnumView> functionIndexToDomain<EnumDomain>(
    const EnumDomain&, const DimensionVec& dimensions, UInt index,
    size_t& dimIndex) {
    debug_code(assert(dimIndex < dimensions.size()));
    auto val = make<EnumValue>();
    val->value = dimensions[dimIndex].lower + index;
    ++dimIndex;
    return val.asExpr();
}

template <>
void functionIndexToDomain<EnumDomain>(const EnumDomain&,
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
ExprRef<TupleView> functionIndexToDomain<TupleDomain>(
    const TupleDomain& domain, const DimensionVec& dimVec, UInt index,
    size_t& dimIndex) {
    vector<AnyExprRef> tupleMembers;

    for (auto& inner : domain.inners) {
        UInt row = index / dimVec[dimIndex].blockSize;
        index %= dimVec[dimIndex].blockSize;
        mpark::visit(
            [&](auto& innerDomainPtr) {
                tupleMembers.emplace_back(functionIndexToDomain(
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
void functionIndexToDomain<TupleDomain>(const TupleDomain& domain,
                                        const DimensionVec& dimensions,
                                        UInt index, TupleView& view,
                                        size_t& dimIndex) {
    debug_code(assert(view.members.size() == domain.inners.size()));
    for (size_t i = 0; i < domain.inners.size(); i++) {
        auto& dim = dimensions[dimIndex];
        mpark::visit(
            [&](auto& innerDomainPtr) {
                typedef
                    typename BaseType<decltype(innerDomainPtr)>::element_type
                        Domain;
                typedef typename AssociatedValueType<Domain>::type Value;
                typedef typename AssociatedViewType<Value>::type View;
                auto& memberExpr = mpark::get<ExprRef<View>>(view.members[i]);
                auto& memberView = memberExpr->view().get();
                UInt row = index / dim.blockSize;
                index %= dim.blockSize;
                functionIndexToDomain(*innerDomainPtr, dimensions, row,
                                      memberView, dimIndex);
            },
            domain.inners[i]);
    }
}

template <>
HashType getValueHash<FunctionView>(const FunctionView& val) {
    return val.cachedHashTotal.getOrSet([&]() {
        return mpark::visit(
            [&](const auto& range) {
                HashType total(0);
                for (size_t i = 0; i < range.size(); ++i) {
                    total += val.calcMemberHash(i, range[i]);
                }
                return total;
            },
            val.range);
    });
}

template <>
ostream& prettyPrint<FunctionView>(ostream& os, const FunctionView& v) {
    os << "function(";
    mpark::visit(
        [&](auto& rangeImpl) {
            for (size_t i = 0; i < rangeImpl.size(); i++) {
                if (i > 0) {
                    os << ",\n";
                }
                mpark::visit(
                    [&](auto& fromDomain) {
                        typedef typename BaseType<decltype(
                            fromDomain)>::element_type Domain;
                        auto from = v.indexToDomain<Domain>(i);
                        prettyPrint(os, from->view());
                    },
                    v.fromDomain);
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
    mpark::visit(
        [&](auto& rangeImpl) {
            typedef
                typename AssociatedValueType<viewType(rangeImpl)>::type Value;
            typedef typename AssociatedDomain<Value>::type Domain;
            const auto& toDomainPtr = mpark::get<shared_ptr<Domain>>(domain.to);

            for (size_t i = 0; i < rangeImpl.size(); i++) {
                if (i > 0) {
                    os << ",";
                }
                if (!domain.isMatrixDomain) {
                    mpark::visit(
                        [&](auto& fromDomain) {
                            typedef typename BaseType<decltype(
                                fromDomain)>::element_type Domain;
                            auto from = v.indexToDomain<Domain>(i);
                            prettyPrint(os, *fromDomain, from->view());
                        },
                        v.fromDomain);

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

template <typename InnerViewType>
void deepCopyImpl(const FunctionValue&,
                  const ExprRefVec<InnerViewType>& srcMemnersImpl,
                  FunctionValue& target) {
    target.cachedHashTotal.invalidate();
    // to be optimised later
    target.silentClear();
    for (size_t i = 0; i < srcMemnersImpl.size(); i++) {
        target.assignImage(i, deepCopy(*assumeAsValue(srcMemnersImpl[i])));
    }
    debug_code(target.debugSanityCheck());
    target.notifyEntireValueChanged();
}

template <>
void deepCopy<FunctionValue>(const FunctionValue& src, FunctionValue& target) {
    assert(src.range.index() == target.range.index());
    return visit(
        [&](auto& srcMembersImpl) {
            return deepCopyImpl(src, srcMembersImpl, target);
        },
        src.range);
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
    mpark::visit(
        [&](auto& srcMembersImpl) {
            target.setInnerType<viewType(srcMembersImpl)>();
        },
        src.range);
}

void matchInnerType(const FunctionDomain& domain, FunctionValue& target) {
    mpark::visit(
        [&](auto& innerDomainImpl) {
            target.setInnerType<typename AssociatedViewType<
                typename AssociatedValueType<typename BaseType<decltype(
                    innerDomainImpl)>::element_type>::type>::type>();
        },
        domain.to);
}

template <>
UInt getDomainSize<FunctionDomain>(const FunctionDomain& domain) {
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
    mpark::visit(
        [&](auto& valMembersImpl) { normaliseImpl(val, valMembersImpl); },
        val.range);
}

template <>
bool smallerValue<FunctionView>(const FunctionView& u, const FunctionView& v);
template <>
bool largerValue<FunctionView>(const FunctionView& u, const FunctionView& v);

template <>
bool smallerValue<FunctionView>(const FunctionView& u, const FunctionView& v) {
    return mpark::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                mpark::get<BaseType<decltype(uMembersImpl)>>(v.range);
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
    return mpark::visit(
        [&](auto& uMembersImpl) {
            auto& vMembersImpl =
                mpark::get<BaseType<decltype(uMembersImpl)>>(v.range);
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

void FunctionView::standardSanityChecksForThisType() const {
    mpark::visit(
        [&](auto& range) {
            cachedHashTotal.applyIfValid([&](const auto& value) {
                HashType checkCachedHashTotal;
                for (size_t i = 0; i < range.size(); i++) {
                    checkCachedHashTotal +=
                        calcMemberHash<viewType(range)>(i, range[i]);
                }
                sanityEqualsCheck(checkCachedHashTotal, value);
            });
        },
        this->range);
}

void FunctionValue::debugSanityCheckImpl() const {
    mpark::visit(
        [&](const auto& valMembersImpl) {
            recurseSanityChecks(valMembersImpl);
            this->standardSanityChecksForThisType();
            // check var bases

            varBaseSanityChecks(*this, valMembersImpl);
        },
        range);
}

AnyExprVec& FunctionValue::getChildrenOperands() { return range; }

void FunctionView::debugCheckDimensionVec() {
    mpark::visit(
        overloaded(
            [&](shared_ptr<TupleDomain>& d) {
                assert(d->inners.size() == dimensions.size());
            },
            [&](shared_ptr<IntDomain>&) { assert(dimensions.size() == 1); },
            [](auto&) { shouldNotBeCalledPanic; }),
        fromDomain);
}
