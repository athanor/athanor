
#ifndef SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#define SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#include <functional>
#include <vector>
#include "types/forwardDecls/typesAndDomains.h"
#include "types/typeTrates.h"
#define assignRandomFunctions(name)                            \
    void assignRandomValueInDomain(const name##Domain& domain, \
                                   name##Value& val);
buildForAllTypes(assignRandomFunctions, )
#undef assignRandomFunctions

    inline void assignRandomValueInDomain(const Domain& domain, Value& val) {
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
struct Neighbourhood {
    typedef std::function<void(const AcceptanceCallBack&,
                               Value& backUpDestination, Value& primary)>
        ApplyFunc;
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

    template <typename DomainPtrType>
    inline void generateNeighbourhoodsImpl(
        const DomainPtrType& domainImpl,
        std::vector<Neighbourhood>& neighbourhoods) {
    for (auto& generator :
         NeighbourhoodGenList<typename DomainPtrType::element_type>::value) {
        generator(*domainImpl, neighbourhoods);
    }
}

inline void generateNeighbourhoods(const Domain domain,
                                   std::vector<Neighbourhood>& neighbourhoods) {
    mpark::visit(
        [&](auto& domainImpl) {
            generateNeighbourhoodsImpl(domainImpl, neighbourhoods);
        },
        domain);
}
#endif /* SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_ */
