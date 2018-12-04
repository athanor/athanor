#include "search/violationContainer.h"
#include <algorithm>
using namespace std;

// defining some extern vars
ViolationContainer emptyViolationContainer;
ViolationContainer& emptyViolations = emptyViolationContainer;

UInt ViolationContainer::calcMinViolation() const {

    if (getVarsWithViolation().empty()) {
        return 0;
    }
    auto varWithMinVio = min_element(
        getVarsWithViolation().begin(), getVarsWithViolation().end(),
        [&](auto i, auto j) {
            return varViolation(i) < varViolation(j);
        });
    return varViolation(*varWithMinVio);
}

UInt ViolationContainer::selectRandomVar(UInt maxVar) const {
    if (totalViolation == 0) {
        return globalRandom<UInt>(0, maxVar);
    }
    double simulatedMinViolation = 0;
    double rand;
    const int numberNonViolatingVars =
        (maxVar + 1) - getVarsWithViolation().size();
    if (numberNonViolatingVars > 0) {
        UInt minViolation = calcMinViolation();

        // we now pretend that the violations that had a violation of 0, now
        // have a violation of min/n  where n=numberNonViolatingVars and
        // min=minViolation.
        simulatedMinViolation = ((double)minViolation) / numberNonViolatingVars;
        // now generate a random number in the range that includes the new
        // simulated min violations

        rand = globalRandom<double>(0, (totalViolation + minViolation) - 1);
    } else {
        rand = globalRandom<UInt>(0, totalViolation - 1);
    }
    for (size_t varId = 0; varId <= maxVar; varId++) {
        double violation = varViolation(varId);
        if (violation == 0) {
            violation = simulatedMinViolation;
        }
        if (violation > rand) {
            return varId;
        } else {
            rand -= violation;
        }
    }
    assert(false);
    abort();
}

vector<UInt> ViolationContainer::selectRandomVars(UInt maxVar,
                                                  size_t numberVars) const {
    vector<UInt> vars;
    UInt violationToIgnore = 0;
    while (vars.size() < numberVars) {
        if (totalViolation == 0 || totalViolation - 1 < violationToIgnore) {
            UInt randomVar = globalRandom<UInt>(0, maxVar);
            if (find(vars.begin(), vars.end(), randomVar) != vars.end()) {
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
                if (find(vars.begin(), vars.end(), varId) != vars.end()) {
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
                cerr << "Failed to select random var\n";
                assert(false);
                abort();
            }
        }
    }
    return vars;
}
