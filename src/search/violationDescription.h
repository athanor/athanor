
#ifndef SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#define SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include "utils/random.h"

class ViolationDescription {
    u_int64_t totalViolation = 0;
    std::vector<u_int64_t> varViolations;
    std::vector<u_int64_t> varsWithViolation;
    std::unordered_map<u_int64_t, std::unique_ptr<ViolationDescription>>
        _childViolations;

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
        totalViolation += violation;
    }

    inline void reset() {
        totalViolation = 0;
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

    inline u_int64_t getTotalViolation() const { return totalViolation; }
    inline ViolationDescription& childViolations(u_int64_t id) {
        auto& ptr = _childViolations[id];
        if (!ptr) {
            ptr = std::make_unique<ViolationDescription>();
        }
        return *ptr;
    }
    inline const ViolationDescription& childViolations(u_int64_t id) const {
        return *_childViolations.at(id);
    }
    inline bool hasChildViolation(u_int64_t id) const {
        return _childViolations.count(id);
    }

    u_int64_t selectRandomVar() const {
        if (totalViolation == 0) {
            std::cerr << "Cannot select random variable when there are zero "
                         "violations\n";
            abort();
        }
        u_int64_t rand = globalRandom<u_int64_t>(0, totalViolation - 1);
        for (u_int64_t varId : getVarsWithViolation()) {
            u_int64_t violation = getVarViolationMapping()[varId];
            if (violation > rand) {
                return varId;
            } else {
                rand -= violation;
            }
        }
        assert(false);
        abort();
    }
};

#endif /* SRC_SEARCH_VIOLATIONDESCRIPTION_H_ */
