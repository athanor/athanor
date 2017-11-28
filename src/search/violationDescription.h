
#ifndef SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#define SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#include <iostream>
#include <unordered_map>
#include <vector>
class ViolationDescription {
    std::vector<u_int64_t> varViolations;
    std::vector<u_int64_t> varsWithViolation;
    std::unordered_map<u_int64_t, ViolationDescription> _childViolations;

   public:
    ViolationDescription(const u_int64_t numberVariables = 0)
        : varViolations(numberVariables, 0) {}
    inline void addViolation(u_int64_t id, u_int64_t violation) {
        if (id >= varViolations.size()) {
            varViolations.resize(id + 1, 0);
        }
        if (varViolations[id] == 0) {
            varsWithViolation.push_back(id);
        }
        varViolations[id] += violation;
    }

    inline void reset() {
        size_t numberVariables = varViolations.size();
        varViolations.clear();
        varsWithViolation.clear();
        _childViolations.clear();
        varViolations.resize(numberVariables, 0);
    }

    inline const std::vector<u_int64_t> getVarViolationMapping() const {
        return varViolations;
    }
    inline const std::vector<u_int64_t> getVarsWithViolation() const {
        return varsWithViolation;
    }

    inline ViolationDescription& childViolations(u_int64_t id) {
        return _childViolations[id];
    }
    inline const ViolationDescription& childViolations(u_int64_t id) const {
        return _childViolations.at(id);
    }
    inline bool hasChildViolation(u_int64_t id) const {
        return _childViolations.count(id);
    }
};

#endif /* SRC_SEARCH_VIOLATIONDESCRIPTION_H_ */
