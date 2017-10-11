#ifndef SRC_SEARCH_MODEL_H_
#define SRC_SEARCH_MODEL_H_
#include "operators/boolOperators.h"
#include "operators/boolProducing.h"
#include "operators/intProducing.h"
#include "types/forwardDecls/typesAndDomains.h"
class ModelBuilder;
enum OptimiseMode { NONE, MAXIMISE, MINIMISE };
class Model {
    friend ModelBuilder;
    std::vector<Value> variables;
    OpAnd csp;
    IntProducing objective;
    OptimiseMode optimiseMode;
    ;

    Model(std::vector<Value> variables, std::vector<BoolProducing> constraints,
          IntProducing objective, OptimiseMode optimiseMode)
        : variables(std::move(variables)),
          csp(std::move(constraints)),
          objective(std::move(objective)),
          optimiseMode(optimiseMode) {}
};

class ModelBuilder {
    std::vector<Value> variables;
    std::vector<BoolProducing> constraints;
    IntProducing objective = construct<IntValue>();  // non applicable default
                                                     // to avoid undefined
                                                     // variant
    OptimiseMode optimiseMode = OptimiseMode::NONE;

   public:
    ModelBuilder() {}

    inline void addConstraint(BoolProducing constraint) {
        constraints.emplace_back(std::move(constraint));
    }
    template <typename ValueType>
    inline ValRef<ValueType> addVariable() {
        variables.emplace_back();
        return variables.back().emplace<ValRef<ValueType>>();
    }
    inline void setObjective(OptimiseMode mode, IntProducing obj) {
        objective = std::move(obj);
        optimiseMode = mode;
        ;
    }
    Model build() {
        return Model(variables, std::move(constraints), std::move(objective),
                     optimiseMode);
    }
};
#endif /* SRC_SEARCH_MODEL_H_ */
