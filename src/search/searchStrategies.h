
#ifndef SRC_SEARCH_SEARCHSTRATEGIES_H_
#define SRC_SEARCH_SEARCHSTRATEGIES_H_

class State;
class SearchStrategy {
   public:
    virtual void run(State& state, bool isOuterMostStrategy) = 0;
    virtual ~SearchStrategy() {}
    virtual inline void printAdditionalStats(std::ostream&) {}
};
#endif /* SRC_SEARCH_SEARCHSTRATEGIES_H_ */
