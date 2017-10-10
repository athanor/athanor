#ifndef SRC_SEARCH_SOLVER_H_
#define SRC_SEARCH_SOLVER_H_
#include "search/model.h"
template <typename SearchStrategy>
class Solver {
    SearchStrategy strategy;
    Model model;
};
#endif /* SRC_SEARCH_SOLVER_H_ */
