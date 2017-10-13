
#ifndef SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#define SRC_SEARCH_NEIGHBOURHOODSELECTIONSTRATEGIES_H_
#include <algorithm>
#include <vector>
#include "utils/random.h"
class NeighbourhoodResult {};
class RandomNeighbourhoodSelection {
    const int numberNeighbourhoods;

   public:
    RandomNeighbourhoodSelection(const int numberNeighbourhoods)
        : numberNeighbourhoods(numberNeighbourhoods) {}

    inline u_int32_t nextNeighbourhood() {
        return globalRandom<u_int32_t>(0, numberNeighbourhoods - 1);
    }
    inline void reportResult(const NeighbourhoodResult&) {}
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
