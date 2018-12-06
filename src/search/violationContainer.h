
#ifndef SRC_SEARCH_VIOLATIONCONTAINER_H_
#define SRC_SEARCH_VIOLATIONCONTAINER_H_
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include "base/intSize.h"
#include "utils/random.h"

class ViolationContainer;
extern ViolationContainer& emptyViolations;
class ViolationContainer {
    UInt totalViolation = 0;
    std::vector<UInt> varViolations;
    std::vector<UInt> varsWithViolation;
    std::unordered_map<UInt, std::unique_ptr<ViolationContainer>>
        _childViolations;

   public:
    ViolationContainer(const UInt numberVariables = 0)
        : varViolations(numberVariables, 0) {}
    inline void addViolation(UInt id, UInt violation) {
        if (violation == 0) {
            return;
        }
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

    inline UInt varViolation(size_t varIndex) const {
        return (varIndex < varViolations.size()) ? varViolations[varIndex] : 0;
    }
    inline const std::vector<UInt>& getVarsWithViolation() const {
        return varsWithViolation;
    }

    inline UInt getTotalViolation() const { return totalViolation; }
    inline ViolationContainer& childViolations(UInt id) {
        auto& ptr = _childViolations[id];
        if (!ptr) {
            ptr = std::make_unique<ViolationContainer>();
        }
        return *ptr;
    }
    inline const ViolationContainer& childViolations(UInt id) const {
        return (hasChildViolation(id)) ? *_childViolations.at(id)
                                       : emptyViolations;
    }
    inline bool hasChildViolation(UInt id) const {
        return _childViolations.count(id);
    }

    UInt selectRandomVar(UInt maxVar) const;
    std::vector<UInt> selectRandomVars(UInt maxVar, size_t numberVars) const;
    UInt calcMinViolation() const;
};

#endif /* SRC_SEARCH_VIOLATIONCONTAINER_H_ */
