
#ifndef SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#define SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#include <functional>
#include <vector>
#include "common/common.h"
#include "search/statsContainer.h"
#include "search/violationDescription.h"
#include "base/base.h"
#include "base/typeOperations.h"

#define debug_neighbourhood_action(x) debug_log(x)

typedef std::function<bool()> AcceptanceCallBack;

typedef std::function<bool(const AnyValRef& newValue)> ParentCheckCallBack;

struct NeighbourhoodParams {
    const AcceptanceCallBack& changeAccepted;
    const ParentCheckCallBack& parentCheck;
    const int parentCheckTryLimit;
    AnyValRef& primary;
    StatsContainer& stats;
    ViolationDescription& vioDesc;
    NeighbourhoodParams(const AcceptanceCallBack& changeAccepted,
                        const ParentCheckCallBack& parentCheck,
                        const int parentCheckTryLimit, AnyValRef& primary,
                        StatsContainer& stats, ViolationDescription& vioDesc)
        : changeAccepted(changeAccepted),
          parentCheck(parentCheck),
          parentCheckTryLimit(parentCheckTryLimit),
          primary(primary),
          stats(stats),
          vioDesc(vioDesc) {}
};

struct Neighbourhood {
    typedef std::function<void(NeighbourhoodParams&)> ApplyFunc;
    std::string name;
    ApplyFunc apply;
    Neighbourhood(std::string&& name, ApplyFunc&& apply)
        : name(std::move(name)), apply(std::move(apply)) {}
};

template <typename Domain>
using NeighbourhoodGenerator = void (*)(const Domain&,
                                        std::vector<Neighbourhood>&);

template <typename Domain>
struct MultiInstanceNeighbourhoodGenerator {
    const int numberInstances;
    NeighbourhoodGenerator<Domain> generator;
};

template <typename Domain>
using NeighbourhoodVec = std::vector<NeighbourhoodGenerator<Domain>>;

template <typename Domain>
using MultiInstanceNeighbourhoodVec =
    std::vector<MultiInstanceNeighbourhoodGenerator<Domain>>;

template <typename DomainType>
struct NeighbourhoodGenList;

template <typename DomainType>
struct MultiInstanceNeighbourhoodGenList;

#define makeGeneratorDecls(name)                                        \
    template <>                                                         \
    struct NeighbourhoodGenList<name##Domain> {                         \
        static const NeighbourhoodVec<name##Domain> value;              \
    };                                                                  \
    template <>                                                         \
    struct MultiInstanceNeighbourhoodGenList<name##Domain> {            \
        static const MultiInstanceNeighbourhoodVec<name##Domain> value; \
    };

buildForAllTypes(makeGeneratorDecls, )
#undef makeGeneratorDecls
    template <typename DomainPtrType>
    inline void generateNeighbourhoodsImpl(
        const DomainPtrType& domainImpl,
        std::vector<Neighbourhood>& neighbourhoods) {
    for (auto& generator :
         NeighbourhoodGenList<typename DomainPtrType::element_type>::value) {
        generator(*domainImpl, neighbourhoods);
    }
}

inline void generateNeighbourhoods(const AnyDomainRef domain,
                                   std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](auto& domainImpl) {
            generateNeighbourhoodsImpl(domainImpl, neighbourhoods);
        },
        domain);
}

template <typename Domain>
void assignRandomValueInDomain(const Domain& domain,
                               typename AssociatedValueType<Domain>::type& val);
template <typename T = int>
inline void assignRandomValueInDomain(const AnyDomainRef& domain,
                                      AnyValRef& val, T = 0) {
    mpark::visit(
        [&](auto& domainImpl) {
            typedef typename BaseType<decltype(domainImpl)>::element_type
                DomainType;
            typedef ValRef<typename AssociatedValueType<DomainType>::type>
                ValueType;
            assignRandomValueInDomain(*domainImpl, *mpark::get<ValueType>(val));
        },
        domain);
}

#endif /* SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_ */
