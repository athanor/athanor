
#ifndef SRC_SEARCH_VIOLATIONCONTAINER_H_
#define SRC_SEARCH_VIOLATIONCONTAINER_H_
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>
#include "base/intSize.h"
#include "utils/random.h"

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
    inline ViolationContainer& childViolations(UInt id) {
        auto& ptr = _childViolations[id];
        if (!ptr) {
            ptr = std::make_unique<ViolationContainer>();
        }
        return *ptr;
    }
    inline const ViolationContainer& childViolations(UInt id) const {
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
    std::vector<UInt> selectRandomVars(size_t size) const {
        assert(totalViolation != 0);
        if (totalViolation == 0) {
            std::cerr << "Cannot select random variable when there are zero "
                         "violations\n";
            abort();
        }
        std::vector<UInt> vars;
        UInt violationToIgnore = 0;
        while (vars.size() < size) {
            UInt rand =
                globalRandom<UInt>(0, totalViolation - 1 - violationToIgnore);
            size_t sizeBackup = vars.size();
            for (UInt varId : getVarsWithViolation()) {
                // using linear find as expecting size to be very small
                if (std::find(vars.begin(), vars.end(), varId) == vars.end()) {
                    continue;
                }
                UInt violation = varViolation(varId);
                if (violation > rand) {
                    vars.emplace_back(varId);
                    violationToIgnore += violation;
                    break;
                } else {
                    rand -= violation;
                }
            }
            if (sizeBackup == vars.size()) {
                std::cerr << "Failed to select random var\n";
                assert(false);
                abort();
            }
        }
        return vars;
    }
};

#endif /* SRC_SEARCH_VIOLATIONCONTAINER_H_ */
