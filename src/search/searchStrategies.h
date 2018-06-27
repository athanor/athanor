
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_
#include "search/model.h"
#include "search/statsContainer.h"
class HillClimber {
   public:
    HillClimber(Model&) {}
    bool acceptSolution(const NeighbourhoodResult& result) {
        return result.getDeltaViolation() <= 0 &&
               result.getDeltaObjective() <= 0;
    }
};

#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
