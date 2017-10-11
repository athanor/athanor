
#ifndef SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#define SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#include <algorithm>
#include <vector>
#include "utils/random.h"
class NeighbourhoodResult {};
class RandomNeighbourhoodSelection {
    std::vector<u_int32_t> availableNeighbourhoods;

   public:
    RandomNeighbourhoodSelection(std::vector<u_int32_t> availableNeighbourhoods)
        : availableNeighbourhoods(std::move(availableNeighbourhoods)) {}

    u_int32_t nextNeighbourhood() {
        return availableNeighbourhoods[globalRandom<u_int32_t>(
            0, availableNeighbourhoods.size() - 1)];
    }
    inline void reportResult(const NeighbourhoodResult&) {}
};
class ExhaustiveRandomNeighbourhoodSelection {
    std::vector<u_int32_t> availableNeighbourhoods;
    size_t currentIndex;

    void reshuffle() {
        currentIndex = 0;
        std::random_shuffle(availableNeighbourhoods.begin(),
                            availableNeighbourhoods.end(),
                            [](int size) { return globalRandom(0, size - 1); });
    }

   public:
    ExhaustiveRandomNeighbourhoodSelection(
        std::vector<u_int32_t> availableNeighbourhoods)
        : availableNeighbourhoods(std::move(availableNeighbourhoods)) {
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
