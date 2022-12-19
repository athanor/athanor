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
                                   UInt maxVar, const double rand,
                                   const double simulatedMinViolation) {
    double consumedInterval = 0;
    for (size_t varId = 0; varId <= maxVar; varId++) {
        double violation = vioCont.varViolation(varId);
        if (violation == 0) {
            violation = simulatedMinViolation;
        }
        consumedInterval += violation;
        if (consumedInterval >= rand) {
            return varId;
        }
    }
    assert(false);
    myAbort();
}

static UInt pickRandomVariable(const ViolationContainer &vioCont, UInt maxVar,
                               UInt minViolation = 0) {
    if (vioCont.getTotalViolation() == 0) {
        return globalRandom<UInt>(0, maxVar);
    }
    double simulatedMinViolation = 0;
    double rand;
    const UInt numberNonViolatingVars =
        (maxVar + 1) - vioCont.getVarsWithViolation().size();
    if (numberNonViolatingVars == 0) {
        rand = globalRandom<double>(0, vioCont.getTotalViolation());
    } else {
        // There are some non violating variables, we now pretend that these
        // variables  now have a violation of min/n where
        // n=numberNonViolatingVars and min= the minimum violation.
        // The idea is that the sum of all the simulated violations cannot be
        // greater than the minimum violation.

        // first calculate min violation, skip if already passed in
        if (minViolation == 0) {
            minViolation = vioCont.calcMinViolation();
        }
        simulatedMinViolation = ((double)minViolation) / numberNonViolatingVars;
        // now generate a random number in the range that includes the new
        // simulated min violations
        rand =
            globalRandom<double>(0, vioCont.getTotalViolation() + minViolation);
    }
    debug_log("max = " << (vioCont.getTotalViolation() + minViolation)
                       << " rand = " << rand);
    return findContainingInterval(vioCont, maxVar, rand, simulatedMinViolation);
}

UInt ViolationContainer::selectRandomVar(UInt maxVar) const {
    debug_code(assert(varViolations.size() <= maxVar + 1));
    UInt randomVar = pickRandomVariable(*this, maxVar);
    debug_code(assert(randomVar <= maxVar));
    return randomVar;
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
