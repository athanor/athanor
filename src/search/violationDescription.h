
#ifndef SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#define SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#include <iostream>
#include <vector>
class ViolationDescription {
    std::vector<u_int64_t> varViolations;
    std::vector<u_int64_t> varsWithViolation;

   public:
    ViolationDescription(const u_int64_t numberVariables)
        : varViolations(numberVariables, 0) {}
    inline void addViolation(u_int64_t id, u_int64_t violation) {
        if (varViolations[id] == 0) {
            varsWithViolation.push_back(id);
        }
        varViolations[id] += violation;
    }

    inline void reset() {
        size_t numberVariables = varViolations.size();
        varViolations.clear();
        varsWithViolation.clear();
        varViolations.resize(numberVariables, 0);
    }

    inline const std::vector<u_int64_t> getVarViolationMapping() const {
        return varViolations;
    }
    inline const std::vector<u_int64_t> getVarsWithViolation() const {
        return varsWithViolation;
    }
};

#endif /* SRC_SEARCH_VIOLATIONDESCRIPTION_H_ */
