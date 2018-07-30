#include "search/violationContainer.h"

UInt ViolationContainer::selectRandomVar(UInt maxVar) const {
    if (totalViolation == 0) {
        return globalRandom<UInt>(0, maxVar);
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

std::vector<UInt> ViolationContainer::selectRandomVars(
    UInt maxVar, size_t numberVars) const {
    std::vector<UInt> vars;
    UInt violationToIgnore = 0;
    while (vars.size() < numberVars) {
        if (totalViolation == 0 || totalViolation - 1 < violationToIgnore) {
            UInt randomVar = globalRandom<UInt>(0, maxVar);
            if (std::find(vars.begin(), vars.end(), randomVar) != vars.end()) {
                continue;
            } else {
                vars.emplace_back(randomVar);
            }
        } else {
            UInt rand =
                globalRandom<UInt>(0, totalViolation - 1 - violationToIgnore);
            size_t sizeBackup = vars.size();
            for (UInt varId : getVarsWithViolation()) {
                // using linear find as expecting size to be very small
                if (std::find(vars.begin(), vars.end(), varId) != vars.end()) {
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
    }
    return vars;
}
