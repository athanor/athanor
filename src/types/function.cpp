#include "types/function.h"
#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>
#include "common/common.h"
#include "types/bool.h"
#include "types/int.h"
#include "types/tuple.h"
#include "utils/ignoreUnused.h"
using namespace std;
namespace {
pair<bool, UInt> translateValueFromDimension(Int value,
                                             const Dimension& dimension) {
    return (value >= dimension.lower && value <= dimension.upper)
               ? pair<bool, UInt>(true, value - dimension.lower)
               : pair<bool, UInt>(false, 0);
}
Int getAsInt(const AnyExprRef& expr) {
    const ExprRef<IntView>* intTest = mpark::get_if<ExprRef<IntView>>(&expr);
    if (intTest) {
        return (*intTest)->view().value;
    }

    const ExprRef<BoolView>* boolTest = mpark::get_if<ExprRef<BoolView>>(&expr);
    if (boolTest) {
        return (*boolTest)->view().violation == 0;
    }
    cerr << "Error: sorry only handling function from int, bool or tuples of "
            "int/bool\n";
    abort();
}
}  // namespace

pair<bool, UInt> FunctionView::domainToIndex(const IntView& intV) {
    debug_code(assert(dimensions.size() == 1));
    return translateValueFromDimension(intV.value, dimensions[0]);
}

pair<bool, UInt> FunctionView::domainToIndex(const TupleView& tupleV) {
    debug_code(assert(dimensions.size() == tupleV.members.size()));
    size_t index = 0;
    for (size_t i = 0; i < tupleV.members.size(); i++) {
        auto boolIndexPair = translateValueFromDimension(
            getAsInt(tupleV.members[i]), dimensions[i]);
        if (!boolIndexPair.first) {
            return boolIndexPair;
        }
        index += dimensions[i].blockSize * boolIndexPair.second;
    }
    debug_code(assert(index < rangeSize()));
    return make_pair(true, index);
}

template <>
ExprRef<IntView> functionIndexToDomain<IntView>(const DimensionVec& dimensions,
                                                UInt index) {
    debug_code(assert(dimensions.size() == 1));
    auto val = make<IntValue>();
    val->value = dimensions[0].lower + index;
    return val.asExpr();
}

template <>
ExprRef<TupleView> functionIndexToDomain<TupleView>(
    const DimensionVec& dimensions, UInt index) {
    auto val = make<TupleValue>();
    for (auto& dim : dimensions) {
        auto intVal = make<IntValue>();
        UInt row = index / dim.blockSize;
        index %= dim.blockSize;
        intVal->value = row + dim.lower;
        val->addMember(intVal);
    }
    return val.asExpr();
}

template <>
HashType getValueHash<FunctionView>(const FunctionView& val) {
    todoImpl(val);
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
                if (v.dimensions.size() == 1) {
                    auto from = v.indexToDomain<IntView>(i);
                    prettyPrint(os, from->view());
                } else {
                    auto from = v.indexToDomain<TupleView>(i);
                    prettyPrint(os, from->view());
                }
                os << " --> ";
                prettyPrint(os, rangeImpl[i]->view());
            }
        },
        v.range);
    os << ")";
    return os;
}

template <typename InnerViewType>
void deepCopyImpl(const FunctionValue&,
                  const ExprRefVec<InnerViewType>& srcMemnersImpl,
                  FunctionValue& target) {
    todoImpl(srcMemnersImpl, target);
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
    os << "function(";
    prettyPrint(os, d.from) << " --> ";
    prettyPrint(os, d.to) << ")";
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
    todoImpl(domain);
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
