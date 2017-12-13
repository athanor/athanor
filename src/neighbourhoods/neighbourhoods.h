
#ifndef SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#define SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#include <functional>
#include <vector>
#include "common/common.h"
#include "search/statsContainer.h"
#include "search/violationDescription.h"
#include "types/base.h"
#include "types/typeOperations.h"

#define debug_neighbourhood_action(x) debug_log(x)
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

template <typename DomainType>
struct NeighbourhoodGenList;
#define makeGeneratorDecls(name)                                   \
    template <>                                                    \
    struct NeighbourhoodGenList<name##Domain> {                    \
        typedef std::vector<void (*)(const name##Domain&,          \
                                     std::vector<Neighbourhood>&)> \
            type;                                                  \
        static const type value;                                   \
    };
buildForAllTypes(makeGeneratorDecls, )
#undef makeGeneratorDecls
    template <typename Domain>
    void assignRandomGen(const Domain& domain,
                         std::vector<Neighbourhood>& neighbourhoods);
template <typename DomainPtrType>
inline void generateNeighbourhoodsImpl(
    const DomainPtrType& domainImpl,
    std::vector<Neighbourhood>& neighbourhoods) {
    for (auto& generator :
         NeighbourhoodGenList<typename DomainPtrType::element_type>::value) {
        generator(*domainImpl, neighbourhoods);
    }
    assignRandomGen(*domainImpl, neighbourhoods);
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
void assignRandomGen(const Domain& domain,
                     std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename AssociatedValueType<Domain>::type ValueType;
    neighbourhoods.emplace_back(
        TypeAsString<ValueType>::value + "AssignRandom",
        [&domain](NeighbourhoodParams& params) {
            auto& val = mpark::get<ValRef<ValueType>>(params.primary);
            auto newMember = constructValueFromDomain(domain);
            AnyValRef newMemberRef(newMember);
            auto backup = deepCopy(*val);
            int numberTries = 0;
            debug_neighbourhood_action(
                "Assigning random value, original value is " << *backup);
            do {
                ++params.stats.minorNodeCount;
                assignRandomValueInDomain(domain, *newMember);
            } while (!params.parentCheck(newMemberRef) &&
                     numberTries++ < params.parentCheckTryLimit);
            if (numberTries >= params.parentCheckTryLimit) {
                debug_neighbourhood_action(
                    "Could not find new value, number tries=" << numberTries);
                return;
            }
            debug_neighbourhood_action("Assigned new value: " << *newMember);
            deepCopy(*newMember, *val);
            if (!params.changeAccepted()) {
                debug_neighbourhood_action("Change rejected");
                deepCopy(*backup, *val);
            }
        });
}

#endif /* SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_ */
