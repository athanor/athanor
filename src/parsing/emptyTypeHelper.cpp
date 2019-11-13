
#include "base/base.h"
#include "parsing/parserCommon.h"
#include "types/allVals.h"
using namespace std;
bool hasEmptyDomain(const AnyDomainRef& domain) {
    auto overload = overloaded(
        [](const shared_ptr<EmptyDomain>&) { return true; },
        [](const shared_ptr<BoolDomain>&) { return false; },
        [](const shared_ptr<IntDomain>&) { return false; },
        [](const shared_ptr<EnumDomain>&) { return false; },
        [](const shared_ptr<TupleDomain>& domain) {
            return find_if(begin(domain->inners), end(domain->inners),
                           hasEmptyDomain) != end(domain->inners);
        },
        [](const shared_ptr<FunctionDomain>& domain) {
            return hasEmptyDomain(domain->to);
        },
        [](auto& domain) { return hasEmptyDomain(domain->inner); });
    return lib::visit(overload, domain);
}
static const char* DOMAIN_MISS_MATCH_MESSAGE =
    "Found a miss match in domain and value type.\n";

template <typename Domain>
void tryRemoveEmptyType(Domain& domain,
                        typename AssociatedViewType<Domain>::type& val);
void tryRemoveEmptyType(const AnyDomainRef& domain, AnyExprRef val) {
    lib::visit(
        [&](const auto& domain) {
            typedef typename BaseType<decltype(domain)>::element_type Domain;
            typedef typename AssociatedViewType<Domain>::type View;
            auto* exprPtr = lib::get_if<ExprRef<View>>(&val);
            if (!exprPtr) {
                myCerr << DOMAIN_MISS_MATCH_MESSAGE;
                shouldNotBeCalledPanic;
            }
            auto view = (*exprPtr)->view();
            if (view) {
                tryRemoveEmptyType(*domain, *view);
            }
        },
        domain);
}

void tryRemoveEmptyType(const AnyDomainRef& domain, AnyExprVec& vals) {
    lib::visit(
        [&](const auto& domain) {
            typedef typename BaseType<decltype(domain)>::element_type Domain;
            typedef typename AssociatedViewType<Domain>::type View;
            if (lib::get_if<ExprRefVec<EmptyView>>(&vals)) {
                vals.emplace<ExprRefVec<View>>();
            } else {
                auto* valsPtr = lib::get_if<ExprRefVec<View>>(&vals);
                if (!valsPtr) {
                    myCerr << DOMAIN_MISS_MATCH_MESSAGE;
                    shouldNotBeCalledPanic;
                }
                for (auto& expr : *valsPtr) {
                    tryRemoveEmptyType(domain, expr);
                }
            }
        },
        domain);
}

template <typename Domain>
void tryRemoveEmptyType(Domain& domain,
                        typename AssociatedViewType<Domain>::type& val) {
    // putting unneeded auto type to prevent instantiation untill type matches
    auto overload = overloaded(
        [](EmptyDomain&, auto&) { shouldNotBeCalledPanic; },
        [](BoolDomain&, auto&) {}, [](IntDomain&, auto&) {},
        [](EnumDomain&, auto&) {},
        [&](TupleDomain& domain, auto& val) {
            for (size_t i = 0; i < domain.inners.size(); i++) {
                tryRemoveEmptyType(domain.inners[i], val.members[i]);
            }
        },
        [&](FunctionDomain& domain, auto& val) {
            tryRemoveEmptyType(domain.to, val.range);
        },
        [&](auto& domain, auto& val) {
            tryRemoveEmptyType(domain.inner, val.getChildrenOperands());
        });
    overload(domain, val);
}
