#include <autoArgParse/argParser.h>
#include <sys/time.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "common/common.h"
#include "jsonModelParser.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/searchStrategies.h"
#include "search/solver.h"
#include "utils/hashUtils.h"

using namespace std;
using namespace AutoArgParse;

ArgParser argParser;
auto& fileArg = argParser.add<Arg<ifstream>>(
    "path_to_file", Policy::MANDATORY, "Path to parameter file.",
    [](const string& path) -> ifstream {
        ifstream file;
        file.open(path);
        if (file.good()) {
            return file;
        } else {
            throw ErrorMessage("Error opening file: " + path);
        }
    });

std::mt19937 globalRandomGenerator;
auto& randomSeedFlag = argParser.add<ComplexFlag>(
    "--randomSeed", Policy::OPTIONAL, "Specify a random seed.");
auto& seedArg = randomSeedFlag.add<Arg<int>>(
    "integer_seed", Policy::MANDATORY,
    "Integer seed to use for random generator.",
    chain(Converter<int>(), [](int value) {
        if (value < 0) {
            throw ErrorMessage("Seed must be greater or equal to 0.");
        } else {
            return value;
        }
    }));
bool runSanityChecks = false;
bool verboseSanityError = false;
auto& sanityCheckFlag = argParser.add<ComplexFlag>(
    "--sanityCheck", Policy::OPTIONAL,
    "Activate sanity check mode, this is a debugging feature,.  After each "
    "move, the state is scanned for errors caused by bad triggering.  Note, "
    "errors caused by hash collisions are not tested for in this mode.",
    [](auto&) { runSanityChecks = true; });

auto& verboseErrorFlag = sanityCheckFlag.add<Flag>(
    "--verbose", Policy::OPTIONAL, "Verbose printing at point of error.",
    [](auto&) { verboseSanityError = true; });

debug_code(bool debugLogAllowed = true;
           auto& disableDebugLoggingFlag = argParser.add<Flag>(
               "--disableDebugLog", Policy::OPTIONAL,
               "Included only for debug builds, can be used to silence logging "
               "but keeping assertions switched on.",
               [](auto&) { debugLogAllowed = false; });
           );
void testHashes() {
    const size_t max = 10000;
    unordered_map<UInt, pair<UInt, UInt>> seenHashes;
    for (UInt i = 0; i < max; ++i) {
        for (UInt j = i; j < max; ++j) {
            HashType hashSum = mix(i) + mix(j);
            if (seenHashes.count(hashSum)) {
                cerr << "Error, found collision:\n";
                cerr << "mix(" << i << ") + mix(" << j << ") collides with ";
                auto& pair = seenHashes[hashSum];
                cerr << "mix(" << pair.first << ") + mix(" << pair.second
                     << ")\nBoth produce total hash of " << hashSum << endl;
                exit(1);
            }
        }
    }
}

auto& exclusiveTimeLimitGroup = argParser.makeExclusiveGroup(Policy::OPTIONAL);
auto& cpuTimeLimitFlag = exclusiveTimeLimitGroup.add<ComplexFlag>(
    "--cpuTimeLimit", "Specify the CPU time limit in seconds.");

auto& cpuTimeLimitArg =
    cpuTimeLimitFlag.add<Arg<int>>("number_seconds", Policy::MANDATORY, "");

auto& realTimeLimitFlag = exclusiveTimeLimitGroup.add<ComplexFlag>(
    "--realTimeLimit", "Specify the CPU time limit in seconds.");

auto& realTimeLimitArg =
    realTimeLimitFlag.add<Arg<int>>("number_seconds", Policy::MANDATORY, "");

u_int64_t iterationLimit;
auto& iterationLimitFlag = argParser.add<ComplexFlag>(
    "--iterationLimit", Policy::OPTIONAL,
    "Specify the maximum number of iterations to spend in search.");

auto& iterationLimitArg = iterationLimitFlag.add<Arg<u_int64_t>>(
    "number_iterations", Policy::MANDATORY, "0 = no limit", [](string& arg) {
        try {
            iterationLimit = stol(arg);
            return iterationLimit;
        } catch (invalid_argument&) {
            throw ErrorMessage("Expected integer >= 0.");
        }
    });

void setTimeout(int numberSeconds, bool virtualTimer);
void sigIntHandler(int);
void sigAlarmHandler(int);

int main(const int argc, const char** argv) {
    argParser.validateArgs(argc, argv);
    ParsedModel parsedModel = parseModelFromJson(fileArg.get());
    State state(parsedModel.builder->build());
    u_int32_t seed = (seedArg) ? seedArg.get() : random_device()();
    globalRandomGenerator.seed(seed);
    cout << "Using seed: " << seed << endl;
    auto nhSelection = make_shared<RandomNeighbourhood>();
    auto strategy = makeExplorationUsingViolationBackOff(
        makeHillClimbing(nhSelection), nhSelection);
    signal(SIGINT, sigIntHandler);
    signal(SIGVTALRM, sigAlarmHandler);
    signal(SIGALRM, sigAlarmHandler);

    if (cpuTimeLimitFlag) {
        setTimeout(cpuTimeLimitArg.get(), true);
    } else if (realTimeLimitFlag) {
        setTimeout(realTimeLimitArg.get(), false);
    }
    try {
        search(strategy, state);
    } catch (SanityCheckException& e) {
        cerr << "***SANITY CHECK ERROR: " << e.errorMessage << endl;
        if (!e.file.empty()) {
            cerr << "From file: " << e.file << endl;
        }
        if (e.line > 0) {
            cerr << "Line: " << e.line << endl;
        }
        if (!e.stateDump.empty()) {
            cerr << "Operator state: " << e.stateDump << endl;
        }
        size_t i = 1;
        for (auto& message : e.messageStack) {
            cerr << i << ": " << message << endl;
            ++i;
        }

        abort();
    }
}

void setTimeout(int numberSeconds, bool virtualTimer) {
    struct itimerval timer;
    // numberSeconds
    timer.it_value.tv_sec = numberSeconds;
    // remaining micro seconds
    timer.it_value.tv_usec = 0;
    // prevent intervals
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;
    if (virtualTimer) {
        setitimer(ITIMER_VIRTUAL, &timer, NULL);
    } else {
        setitimer(ITIMER_REAL, &timer, NULL);
    }
}
bool sigIntActivated = false, sigAlarmActivated = false;
void forceExit() {
    std::cout << "\n\nFORCE EXIT\n";
    abort();
}
void sigIntHandler(int) {
    if (sigIntActivated) {
        forceExit();
    }
    sigIntActivated = true;
    setTimeout(5, false);
}
void sigAlarmHandler(int) {
    if (sigIntActivated || sigAlarmActivated) {
        forceExit();
    }
    sigAlarmActivated = true;
    setTimeout(5, false);
}
