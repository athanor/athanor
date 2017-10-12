
#ifndef SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#define SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_
#include <functional>
#include <vector>
#include "types/forwardDecls/typesAndDomains.h"
#define assignRandomFunctions(name)                            \
    void assignRandomValueInDomain(const name##Domain& domain, \
                                   name##Value& val);
buildForAllTypes(assignRandomFunctions, )
#undef assignRandomFunctions

    typedef std::function<bool()> AcceptanceCallBack;
struct Neighbourhood {
    std::string name;
    std::function<void(const AcceptanceCallBack&)> apply;
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
    inline std::vector<Neighbourhood> generateNeighbourhoodsImpl(
        const DomainPtrType& domainImpl) {
    std::vector<Neighbourhood> neighbourhoods;
    for (auto& generator :
         NeighbourhoodGenList<typename DomainPtrType::element_type>::value) {
        generator(*domainImpl, neighbourhoods);
    }
    return neighbourhoods;
}

inline std::vector<Neighbourhood> generateNeighbourhoods(const Domain domain) {
    return mpark::visit(
        [](auto& domainImpl) { return generateNeighbourhoodsImpl(domainImpl); },
        domain);
}
#endif /* SRC_NEIGHBOURHOODS_NEIGHBOURHOODS_H_ */
