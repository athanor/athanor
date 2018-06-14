

#ifndef SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#define SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#include <functional>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "search/statsContainer.h"
#include "search/violationContainer.h"

#define debug_neighbourhood_action(x) debug_log(x)

typedef std::function<bool()> AcceptanceCallBack;

typedef std::function<bool(const AnyValVec& newValue)> ParentCheckCallBack;

struct NeighbourhoodParams {
    const AcceptanceCallBack& changeAccepted;
    const ParentCheckCallBack& parentCheck;
    const int parentCheckTryLimit;
    AnyValVec& vals;
    StatsContainer& stats;
    ViolationContainer& vioDesc;
    NeighbourhoodParams(const AcceptanceCallBack& changeAccepted,
                        const ParentCheckCallBack& parentCheck,
                        const int parentCheckTryLimit, AnyValVec& vals,
                        StatsContainer& stats, ViolationContainer& vioDesc)
        : changeAccepted(changeAccepted),
          parentCheck(parentCheck),
          parentCheckTryLimit(parentCheckTryLimit),
          vals(vals),
          stats(stats),
          vioDesc(vioDesc) {}

    template <typename Val>
    ValRefVec<Val>& getVals() {
        return mpark::get<ValRefVec<Val>>(vals);
    }
};

struct Neighbourhood {
    typedef std::function<void(NeighbourhoodParams&)> ApplyFunc;
    std::string name;
    int numberValsRequired;
    ApplyFunc apply;
    Neighbourhood(std::string name, int numberValsRequired, ApplyFunc apply)
        : name(std::move(name)),
          numberValsRequired(numberValsRequired),
          apply(std::move(apply)) {}
};

template <typename Domain>
using GeneratorFunc = void (*)(const Domain&, int, std::vector<Neighbourhood>&);

template <typename Domain>
struct NeighbourhoodGenerator {
    int numberValsRequired;
    GeneratorFunc<Domain> generate;
};

template <typename Domain>
using NeighbourhoodVec = std::vector<NeighbourhoodGenerator<Domain>>;

template <typename DomainType>
struct NeighbourhoodGenList;

#define makeGeneratorDecls(name)                           \
    template <>                                            \
    struct NeighbourhoodGenList<name##Domain> {            \
        static const NeighbourhoodVec<name##Domain> value; \
    };
buildForAllTypes(makeGeneratorDecls, )
#undef makeGeneratorDecls
    template <typename DomainPtrType>
    inline void generateNeighbourhoodsImpl(
        int maxNumberVals, const DomainPtrType& domainImpl,
        std::vector<Neighbourhood>& neighbourhoods) {
    for (auto& generator :
         NeighbourhoodGenList<typename DomainPtrType::element_type>::value) {
        if (generator.numberValsRequired <= maxNumberVals) {
            generator.generate(*domainImpl, generator.numberValsRequired,
                               neighbourhoods);
        }
    }
}

inline void generateNeighbourhoods(int maxNumberVals, const AnyDomainRef domain,
                                   std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](auto& domainImpl) {
            generateNeighbourhoodsImpl(maxNumberVals, domainImpl,
                                       neighbourhoods);
        },
        domain);
}

template <typename Domain>
void assignRandomValueInDomain(const Domain& domain,
                               typename AssociatedValueType<Domain>::type& val,
                               StatsContainer& stats);
template <typename T = int>
inline void assignRandomValueInDomain(const AnyDomainRef& domain,
                                      AnyValRef& val, StatsContainer& stats,
                                      T = 0) {
    mpark::visit(
        [&](auto& domainImpl) {
            typedef typename BaseType<decltype(domainImpl)>::element_type
                DomainType;
            typedef ValRef<typename AssociatedValueType<DomainType>::type>
                ValueType;
            assignRandomValueInDomain(*domainImpl, *mpark::get<ValueType>(val),
                                      stats);
        },
        domain);
}

#endif /* SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_ */
