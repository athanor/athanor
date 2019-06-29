
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
    return mpark::visit(overload, domain);
}
template <typename Domain>
void removeEmptyType(Domain& domain,
                     typename AssociatedViewType<Domain>::type& val);
void removeEmptyType(const AnyDomainRef& domain, AnyExprRef val) {
    mpark::visit(
        [&](const auto& domain) {
            typedef typename BaseType<decltype(domain)>::element_type Domain;
            typedef typename AssociatedViewType<Domain>::type View;
            auto view = mpark::get<ExprRef<View>>(val)->view();
            if (view) {
                removeEmptyType(*domain, *view);
            }
        },
        domain);
}
void removeEmptyType(const AnyDomainRef& domain, AnyExprVec& vals) {
    mpark::visit(
        [&](const auto& domain) {
            typedef typename BaseType<decltype(domain)>::element_type Domain;
            typedef typename AssociatedViewType<Domain>::type View;
            if (mpark::get_if<ExprRefVec<EmptyView>>(&vals)) {
                vals.emplace<ExprRefVec<View>>();
            } else {
                for (auto& expr : mpark::get<ExprRefVec<View>>(vals)) {
                    removeEmptyType(domain, expr);
                }
            }
        },
        domain);
}

template <typename Domain>
void removeEmptyType(Domain& domain,
                     typename AssociatedViewType<Domain>::type& val) {
    // putting unneeded auto type to prevent instantiation untill type matches
    auto overload =
        overloaded([](EmptyDomain&, auto&&) { shouldNotBeCalledPanic; },
                   [](BoolDomain&, auto&&) {}, [](IntDomain&, auto&&) {},
                   [](EnumDomain&, auto&) {},
                   [&](TupleDomain& domain, auto& val) {
                       for (size_t i = 0; i < domain.inners.size(); i++) {
                           removeEmptyType(domain.inners[i], val.members[i]);
                       }
                   },
                   [&](FunctionDomain& domain, auto& val) {
                       removeEmptyType(domain.to, val.range);
                   },
                   [&](auto& domain, auto& val) {
                       removeEmptyType(domain.inner, val.members);
                   });
    overload(domain, val);
}
