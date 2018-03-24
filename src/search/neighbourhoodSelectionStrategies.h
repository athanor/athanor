
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
    UInt bestViolation;
    NeighbourhoodResult(Model& model, UInt bestViolation)
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

   public:
    RandomNeighbourhoodWithViolation(const int numberNeighbourhoods)
        : numberNeighbourhoods(numberNeighbourhoods) {}

    inline u_int32_t nextNeighbourhood(const Model& model) {
        if (model.vioDesc.getTotalViolation() == 0) {
            return globalRandom<int>(0, numberNeighbourhoods - 1);
        } else {
            return selectNeighbourhoodFromVarId(
                model, model.vioDesc.selectRandomVar());
        }
    }

    inline void initialise(Model& model) {
        model.vioDesc.reset();
        if (model.csp.violation == 0) {
            return;
        }
        model.csp.updateViolationDescription(0, model.vioDesc);
    }
    inline void reportResult(NeighbourhoodResult&& result) {
        initialise(result.model);
    }
    inline int selectNeighbourhoodFromVarId(const Model& model,
                                            UInt varId) {
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
    inline void reportResult(NeighbourhoodResult&&) {
        if (currentIndex == availableNeighbourhoods.size() - 1) {
            reshuffle();
        } else {
            ++currentIndex;
        }
    }
};

class InteractiveNeighbourhoodSelector {
    const int numberNeighbourhoods;

   public:
    InteractiveNeighbourhoodSelector(const int numberNeighbourhoods)
        : numberNeighbourhoods(numberNeighbourhoods) {}

    inline u_int32_t nextNeighbourhood(const Model& model) {
        while (true) {
            std::cout << "Select a neighbourhood:\n";
            for (int i = 0; i < numberNeighbourhoods; ++i) {
                std::cout << "Option " << i << ": "
                          << model.neighbourhoods[i].name
                          << ", operates on variable with violation="
                          << model.vioDesc.varViolation(
                                 model.neighbourhoodVarMapping[i])
                          << ".\n";
            }
            int index;
            bool readInt(std::cin >> index);
            if (!readInt || index < 0 ||
                index >= (int)model.neighbourhoods.size()) {
                std::cout << "Error, please enter an integer in the range 0.."
                          << (model.neighbourhoods.size() - 1) << ".\n";
            } else {
                return index;
            }
        }
    }

    inline void initialise(Model& model) {
        model.vioDesc.reset();
        if (model.csp.violation == 0) {
            return;
        }
        model.csp.updateViolationDescription(0, model.vioDesc);
    }
    inline void reportResult(NeighbourhoodResult&& result) {
        initialise(result.model);
    }
};

#endif /* SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_ */
