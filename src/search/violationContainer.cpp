#include "search/violationContainer.h"
#include <algorithm>
using namespace std;

// defining some extern vars
ViolationContainer emptyViolationContainer;
ViolationContainer &emptyViolations = emptyViolationContainer;

UInt ViolationContainer::calcMinViolation() const {
    if (getVarsWithViolation().empty()) {
        return 0;
    }
    auto varWithMinVio = min_element(
        getVarsWithViolation().begin(), getVarsWithViolation().end(),
        [&](auto i, auto j) { return varViolation(i) < varViolation(j); });
    return varViolation(*varWithMinVio);
}

static UInt findContainingInterval(const ViolationContainer &vioCont,
                                   UInt maxVar, double rand,
                                   double simulatedMinViolation) {
    for (size_t varId = 0; varId <= maxVar; varId++) {
        double violation = vioCont.varViolation(varId);

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

static UInt pickRandomVariable(const ViolationContainer &vioCont, UInt maxVar,
                               UInt minViolation = 0) {
    if (vioCont.getTotalViolation() == 0) {
        return globalRandom<UInt>(0, maxVar);
    }
    double simulatedMinViolation = 0;
    double rand;
    const int numberNonViolatingVars =
        (maxVar + 1) - vioCont.getVarsWithViolation().size();
    if (numberNonViolatingVars > 0) {
        if (minViolation == 0) {
            // not been calculated before, calculate now
            minViolation = vioCont.calcMinViolation();
        }
        // we now pretend that the violations that had a violation of 0, now
        // have a violation of min/n where n=numberNonViolatingVars and
        // min=minViolation.
        // does not work with n=1, in this case n defaults to 2
        UInt n = (numberNonViolatingVars != 1) ? numberNonViolatingVars : 2;
        simulatedMinViolation = ((double)minViolation) / n;
        // now generate a random number in the range that includes the new
        // simulated min violations
        double additionalVio =
            (numberNonViolatingVars != 1) ? minViolation : ((double)minViolation) / 2;
        rand = globalRandom<double>(0, (vioCont.getTotalViolation() + additionalVio) - 1);
    } else {
        rand = globalRandom<UInt>(0, vioCont.getTotalViolation() - 1);
    }
    return findContainingInterval(vioCont, maxVar, rand, simulatedMinViolation);
}

UInt ViolationContainer::selectRandomVar(UInt maxVar) const {
    return pickRandomVariable(*this, maxVar);
}

vector<UInt> ViolationContainer::selectRandomVars(UInt maxVar,
                                                  size_t numberVars) const {
    const UInt numberNonViolatingVars =
        (maxVar + 1) - getVarsWithViolation().size();
    UInt minViolation = (numberNonViolatingVars == 0) ? 0 : calcMinViolation();
    vector<UInt> vars;
    while (vars.size() < numberVars) {
        UInt var = pickRandomVariable(*this, maxVar, minViolation);
        if (std::find(vars.begin(), vars.end(), var) == vars.end()) {
            vars.emplace_back(var);
        }
    }
    return vars;
}
