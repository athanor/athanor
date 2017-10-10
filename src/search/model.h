#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include "operators/boolProducing.h"
#include "operators/intProducing.h"
#include "types/forwardDecls/typesAndDomains.h"

class Model {
    std::vector<Value> variables;
    BoolProducing csp;
    IntProducing objective;
};

#endif /* SRC_SEARCH_MODEL_H_ */
