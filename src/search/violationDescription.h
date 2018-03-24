
#ifndef SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#define SRC_SEARCH_VIOLATIONDESCRIPTION_H_
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include "base/intSize.h"
#include "utils/random.h"

class ViolationDescription {
    UInt totalViolation = 0;
    std::vector<UInt> varViolations;
    std::vector<UInt> varsWithViolation;
    std::unordered_map<UInt, std::unique_ptr<ViolationDescription>>
        _childViolations;

   public:
    ViolationDescription(const UInt numberVariables = 0)
        : varViolations(numberVariables, 0) {}
    inline void addViolation(UInt id, UInt violation) {
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
    inline const std::vector<UInt> getVarsWithViolation() const {
        return varsWithViolation;
    }

    inline UInt getTotalViolation() const { return totalViolation; }
    inline ViolationDescription& childViolations(UInt id) {
        auto& ptr = _childViolations[id];
        if (!ptr) {
            ptr = std::make_unique<ViolationDescription>();
        }
        return *ptr;
    }
    inline const ViolationDescription& childViolations(UInt id) const {
        return *_childViolations.at(id);
    }
    inline bool hasChildViolation(UInt id) const {
        return _childViolations.count(id);
    }

    UInt selectRandomVar() const {
        assert(totalViolation != 0);
        if (totalViolation == 0) {
            std::cerr << "Cannot select random variable when there are zero "
                         "violations\n";
            abort();
        }
        UInt rand = globalRandom<UInt>(0, totalViolation - 1);
        for (UInt varId : getVarsWithViolation()) {
            UInt violation = varViolation(varId);
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
