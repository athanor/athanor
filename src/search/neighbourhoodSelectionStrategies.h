
#ifndef SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#define SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#include <algorithm>
#include <cassert>
#include <vector>
#include "search/model.h"
#include "search/violationDescription.h"
#include "utils/random.h"
struct NeighbourhoodResult {
    Model& model;
    u_int64_t bestViolation;
    NeighbourhoodResult(Model& model, u_int64_t bestViolation)
        : model(model), bestViolation(bestViolation) {}
};
class RandomNeighbourhoodSelection {
    const int numberNeighbourhoods;

   public:
    RandomNeighbourhoodSelection(const int numberNeighbourhoods)
        : numberNeighbourhoods(numberNeighbourhoods) {
        assert(this->numberNeighbourhoods > 0);
    }

    inline u_int32_t nextNeighbourhood(const Model&) {
        return globalRandom<u_int32_t>(0, numberNeighbourhoods - 1);
    }
    inline void initialise(const Model&) {}
    inline void reportResult(const NeighbourhoodResult&) {}
};
class RandomNeighbourhoodWithViolation {
    const int numberNeighbourhoods;
    ViolationDescription vioDesc;

   public:
    RandomNeighbourhoodWithViolation(const int numberNeighbourhoods)
        : numberNeighbourhoods(numberNeighbourhoods),
          vioDesc(numberNeighbourhoods) {}

    inline u_int32_t nextNeighbourhood(const Model& model) {
        u_int64_t totalViolation = 0;
        for (u_int64_t varId : vioDesc.getVarsWithViolation()) {
            totalViolation += vioDesc.getVarViolationMapping()[varId];
        }
        if (totalViolation == 0) {
            return globalRandom<int>(0, numberNeighbourhoods - 1);
        }
        u_int64_t rand = globalRandom<u_int32_t>(0, totalViolation - 1);
        for (u_int64_t varId : vioDesc.getVarsWithViolation()) {
            u_int64_t violation = vioDesc.getVarViolationMapping()[varId];
            if (violation > rand) {
                return selectNeighbourhoodFromVarId(model, varId);
                ;
            } else {
                rand -= violation;
            }
        }
        assert(false);
        abort();
    }

    inline void initialise(const Model& model) {
        vioDesc.reset();
        updateViolationDescription(model.csp, 0, vioDesc);
    }
    inline void reportResult(const NeighbourhoodResult& result) {
        initialise(result.model);
    }
    inline int selectNeighbourhoodFromVarId(const Model& model,
                                            u_int64_t varId) {
        return model.varNeighbourhoodMapping[varId][globalRandom<size_t>(
            0, model.varNeighbourhoodMapping[varId].size() - 1)];
    }
};
class ExhaustiveRandomNeighbourhoodSelection {
    std::vector<u_int32_t> availableNeighbourhoods;
    size_t currentIndex;

    inline void reshuffle() {
        currentIndex = 0;
        std::random_shuffle(availableNeighbourhoods.begin(),
                            availableNeighbourhoods.end(),
                            [](int size) { return globalRandom(0, size - 1); });
    }

   public:
    ExhaustiveRandomNeighbourhoodSelection(const int numberNeighbourhoods)
        : availableNeighbourhoods(numberNeighbourhoods) {
        std::iota(availableNeighbourhoods.begin(),
                  availableNeighbourhoods.end(), 0);
        reshuffle();
    }

    inline u_int32_t nextNeighbourhood() {
        return availableNeighbourhoods[currentIndex];
    }
    inline void reportResult(const NeighbourhoodResult&) {
        if (currentIndex == availableNeighbourhoods.size() - 1) {
            reshuffle();
        } else {
            ++currentIndex;
        }
    }
};
#endif /* SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_ */
