

#ifndef SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#define SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#include <functional>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "search/violationContainer.h"

#define debug_neighbourhood_action(x) debug_log(x)
struct StatsContainer;
typedef std::function<bool()> AcceptanceCallBack;

typedef std::function<bool(const AnyValVec& newValue)> ParentCheckCallBack;

struct NeighbourhoodParams {
    const AcceptanceCallBack& changeAccepted;
    const ParentCheckCallBack& parentCheck;
    const int parentCheckTryLimit;
    AnyValVec& vals;
    StatsContainer& stats;
    ViolationContainer& vioContainer;
    std::vector<ViolationContainer*>
        vioContainers;  // only for neighbourhoods who require multiple vars

    NeighbourhoodParams(const AcceptanceCallBack& changeAccepted,
                        const ParentCheckCallBack& parentCheck,
                        const int parentCheckTryLimit, AnyValVec& vals,
                        StatsContainer& stats, ViolationContainer& vioContainer)
        : changeAccepted(changeAccepted),
          parentCheck(parentCheck),
          parentCheckTryLimit(parentCheckTryLimit),
          vals(vals),
          stats(stats),
          vioContainer(vioContainer) {}

    template <typename Val>
    ValRefVec<Val>& getVals() {
        return mpark::get<ValRefVec<Val>>(vals);
    }
};

/* a neighbourhood: which is basically a name and a function that can be
 * invokved with neighbourhood parameters */
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

/* Neighbourhood func base class, a template class that can optionally be used
 * to create neighbourhood ApplyFuncs, see the neighbourhood class.  All it does
 * is gives the basic typedefs and references to useful info like domains. */
template <typename DomainType, size_t numberVals, typename Derived>
struct NeighbourhoodFunc {
    typedef DomainType Domain;
    typedef typename AssociatedValueType<Domain>::type Val;

    template <size_t index, typename... Args>
    typename std::enable_if<(index == numberVals), void>::type invokeApply(
        NeighbourhoodParams& params, ValRefVec<Val>&, Args&... args) {
        return static_cast<Derived&>(*this).apply(params, args...);
    }

    template <size_t index, typename... Args>
    typename std::enable_if<(index < numberVals), void>::type invokeApply(
        NeighbourhoodParams& params, ValRefVec<Val>& vals, Args&... args) {
        return invokeApply<index + 1>(params, vals, args..., *vals[index]);
    }

    void operator()(NeighbourhoodParams& params) {
        auto& vals = params.getVals<Val>();
        debug_code(assert(numberVals < vals.size()));
        invokeApply<0>(params, vals);
    }
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

#define makeGeneratorDecls(name)                                         \
    template <>                                                          \
    struct NeighbourhoodGenList<name##Domain> {                          \
        static const NeighbourhoodVec<name##Domain> value;               \
        static const NeighbourhoodVec<name##Domain> mergeNeighbourhoods; \
        static const NeighbourhoodVec<name##Domain> splitNeighbourhoods; \
    };
buildForAllTypes(makeGeneratorDecls, )
#undef makeGeneratorDecls
    template <typename DomainPtrType>
    inline void generateNeighbourhoodsImpl(
        int maxNumberVals, const DomainPtrType& domainImpl,
        std::vector<Neighbourhood>& neighbourhoods) {
    for (auto& generator :
         NeighbourhoodGenList<typename DomainPtrType::element_type>::value) {
        if (maxNumberVals == 0 ||
            generator.numberValsRequired <= maxNumberVals) {
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

template <typename Domain>
const AnyDomainRef getInner(const Domain& domain);
template <typename Domain, template <typename> class NhFunc>
void generateForAllTypes(const Domain& domain, int numberValsRequired,
                         std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](const auto& innerDomainPtr) {
            typedef typename BaseType<decltype(innerDomainPtr)>::element_type
                InnerDomain;
            typedef NhFunc<InnerDomain> Nh;
            if (Nh::matches(domain)) {
                neighbourhoods.emplace_back(Nh::getName(), numberValsRequired,
                                            Nh(domain));
            }
        },
        getInner(domain));
}

template <typename Domain, typename NhFunc>
void generate(const Domain& domain, int numberValsRequired,
              std::vector<Neighbourhood>& neighbourhoods) {
    typedef typename NhFunc::InnerDomainType InnerDomain;
    mpark::visit(overloaded(
                     [&](const std::shared_ptr<InnerDomain>&) {
                         if (NhFunc::matches(domain)) {
                             neighbourhoods.emplace_back(NhFunc::getName(),
                                                         numberValsRequired,
                                                         NhFunc(domain));
                         }
                     },
                     [](const auto&) {}),
                 getInner(domain));
}

static const int NUMBER_TRIES_CONSTANT_MULTIPLIER = 2;

inline int calcNumberInsertionAttempts(UInt numberMembers, UInt domainSize) {
    double successChance = (domainSize - numberMembers) / (double)domainSize;
    if (successChance == 0) {
        return 0;
    }
    return (int)(ceil(NUMBER_TRIES_CONSTANT_MULTIPLIER / successChance));
}

template <typename Val>
void swapValAssignments(Val& val1, Val& val2);
#endif /* SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_ */
