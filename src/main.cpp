#include <autoArgParse/argParser.h>
#include <sys/time.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include "common/common.h"
#include "gitRevision.h"
#include "jsonModelParser.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/searchStrategies.h"
#include "search/solver.h"
#include "utils/hashUtils.h"

using namespace std;
using namespace AutoArgParse;

ArgParser argParser;
auto& specFlag = argParser.add<ComplexFlag>(
    "--spec", Policy::MANDATORY, "Precedes essence specification file.");
auto& specArg = specFlag.add<Arg<ifstream>>(
    "path_to_file", Policy::MANDATORY,
    "JSON representation of an essence file.",
    [](const string& path) -> ifstream {
        ifstream file;
        file.open(path);
        if (file.good()) {
            return file;
        } else {
            throw ErrorMessage("Error opening file: " + path);
        }
    });

auto& paramFlag = argParser.add<ComplexFlag>("--param", Policy::OPTIONAL,
                                             "Precedes parameter file.");
auto& paramArg = paramFlag.add<Arg<ifstream>>(
    "path_to_file", Policy::MANDATORY,
    "JSON representation of an essence parameter file.",
    [](const string& path) -> ifstream {
        ifstream file;
        file.open(path);
        if (file.good()) {
            return file;
        } else {
            throw ErrorMessage("Error opening file: " + path);
        }
    });

mt19937 globalRandomGenerator;
auto& randomSeedFlag = argParser.add<ComplexFlag>(
    "--randomSeed", Policy::OPTIONAL, "Specify a random seed.");
auto& seedArg =
    randomSeedFlag.add<Arg<int>>("integer_seed", Policy::MANDATORY,
                                 "Integer seed to use for random generator.");
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
               [](auto&) { debugLogAllowed = false; }););

enum ImproveStrategyChoice { HILL_CLIMBING, TEST_CLIMB };
enum ExploreStrategyChoice {
    VIOLATION_BACKOFF,
    RANDOM_WALK,
    NO_EXPLORE,
    TEST_EXPLORE
};

enum SelectionStrategyChoice { RANDOM, UCB, UCB_WITH_TRIGGER, INTERACTIVE };

ImproveStrategyChoice improveStrategyChoice = HILL_CLIMBING;
ExploreStrategyChoice exploreStrategyChoice = RANDOM_WALK;
SelectionStrategyChoice selectionStrategyChoice = UCB;

double DEFAULT_UCB_EXPLORATION_BIAS = 1;

auto& improveStratGroup =
    argParser
        .add<ComplexFlag>("--improve", Policy::OPTIONAL,
                          "Specify the strategy used to improve on an "
                          "assignment. (default=hc).")
        .makeExclusiveGroup(Policy::MANDATORY);

auto& hillClimbingFlag = improveStratGroup.add<Flag>(
    "hc", "Hill climbing strategy.",
    [](auto&&) { improveStrategyChoice = HILL_CLIMBING; });

auto& testClimbFlag = improveStratGroup.add<Flag>(
    "test", "Test strategy for developers, use at your own risk.",
    [](auto&&) { improveStrategyChoice = TEST_CLIMB; });

auto& exploreStratGroup =
    argParser
        .add<ComplexFlag>("--explore", Policy::OPTIONAL,
                          "Specify the strategy used to explore when the "
                          "improve strategy fails to improve on an assignment. "
                          "(default=rw).")
        .makeExclusiveGroup(Policy::MANDATORY);

auto& randomWalkFlag = exploreStratGroup.add<Flag>(
    "rw", "Random walk.", [](auto&&) { exploreStrategyChoice = RANDOM_WALK; });

auto& vioBackOffFlag = exploreStratGroup.add<Flag>(
    "vb", "Violation backoff.",
    [](auto&&) { exploreStrategyChoice = VIOLATION_BACKOFF; });

auto& noExploreFlag = exploreStratGroup.add<Flag>(
    "none",
    "Do not use an exploration strategy, only use the specified improve "
    "strategy.",
    [](auto&&) { exploreStrategyChoice = NO_EXPLORE; });

auto& testExploreFlag = exploreStratGroup.add<Flag>(
    "test", "Test exploration strategy for developers, use at your own risk.",
    [](auto&&) { exploreStrategyChoice = TEST_EXPLORE; });

auto& selectionStratGroup =
    argParser
        .add<ComplexFlag>("--selection", Policy::OPTIONAL,
                          "Specify neighbourhood selection strategy "
                          "(default=ucb).")
        .makeExclusiveGroup(Policy::MANDATORY);

auto& randomFlag = selectionStratGroup.add<Flag>(
    "r", "random, select neighbourhoods randomly.",
    [](auto&&) { selectionStrategyChoice = RANDOM; });

auto& ucbFlag = selectionStratGroup.add<Flag>(
    "ucb",
    "Upper confidence bound, a multiarmed bandet method for learning which "
    "neighbourhoods are best performing.",
    [](auto&&) { selectionStrategyChoice = UCB; });

auto& ucbWithTriggerFlag = selectionStratGroup.add<Flag>(
    "ucbWithTrigger",
    "Experimental, like flag \"ucb\" but cost measurements include trigger "
    "counts.",
    [](auto&&) { selectionStrategyChoice = UCB_WITH_TRIGGER; });

auto& interactiveFlag = selectionStratGroup.add<Flag>(
    "i", "interactive, Prompt user for neighbourhood to select.",
    [](auto&&) { selectionStrategyChoice = INTERACTIVE; });

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

auto& disableVioBiasFlag = argParser.add<Flag>(
    "--disableVioBias", Policy::OPTIONAL,
    "Disable the search from biasing towards violating variables.");
extern bool noPrintSolutions;
bool noPrintSolutions = false;
auto& noPrintSolutionsFlag = argParser.add<Flag>(
    "--noPrintSolutions", Policy::OPTIONAL,
    "Do not print solutions, useful for timing experiements.",
    [](auto&) { noPrintSolutions = true; });

ofstream outFileChecker(const string& path) {
    ofstream os(path);
    if (os.good()) {
        return os;
    } else {
        throw ErrorMessage("Could not open file " + path);
    }
}

auto& saveNeighbourhoodStatsArg =
    argParser
        .add<ComplexFlag>(
            "--saveNhStats", Policy::OPTIONAL,
            "Print neighbourhood stats into a file instead of stdout.\n")
        .add<Arg<ofstream>>("path_to_file", Policy::MANDATORY, "",
                            outFileChecker);

auto& saveUcbArg =
    argParser
        .add<ComplexFlag>("--saveUcbState", Policy::OPTIONAL,
                          "Save learned neighbourhood UCB info.\n")
        .add<Arg<ofstream>>("path_to_file", Policy::MANDATORY,
                            "File to save results to.", outFileChecker);

void setTimeout(int numberSeconds, bool virtualTimer);
void sigIntHandler(int);
void sigAlarmHandler(int);

template <typename ImproveStrategy, typename SelectionStrategy>
void constructStratAndRunImpl(State& state, ImproveStrategy&& improve,
                              SelectionStrategy&& nhSelection) {
    switch (exploreStrategyChoice) {
        case VIOLATION_BACKOFF: {
            auto strategy =
                makeExplorationUsingViolationBackOff(improve, nhSelection);
            search(strategy, state);
            return;
        }
        case RANDOM_WALK: {
            auto strategy = makeExplorationUsingRandomWalk(improve);
            search(strategy, state);
            return;
        }
        case NO_EXPLORE: {
            search(improve, state);
            return;
        }
        case TEST_EXPLORE: {
            auto strategy = makeTestExploration(improve);
            search(strategy, state);
            return;
        }
        default:
            abort();
    }
}

template <typename SelectionStrategy>
void constructStratAndRun(State& state, SelectionStrategy&& nhSelection) {
    switch (improveStrategyChoice) {
        case HILL_CLIMBING: {
            constructStratAndRunImpl(state, makeHillClimbing(nhSelection),
                                     nhSelection);
            return;
        }
        case TEST_CLIMB: {
            constructStratAndRunImpl(state, makeTestClimb(nhSelection),
                                     nhSelection);
            return;
        }
        default:
            abort();
    }
}

template <typename T>
void saveUcbResults(const State& state, const T& ucb) {
    if (!saveUcbArg) {
        return;
    }
    auto& os = saveUcbArg.get();
    auto totalCost = ucb->totalCost();
    os << "totalCost," << totalCost << endl;
    csvRow(os, "name", "reward", "cost", "ucbValue");
    for (size_t i = 0; i < state.model.neighbourhoods.size(); i++) {
        csvRow(
            os, state.model.neighbourhoods[i].name, ucb->reward(i),
            ucb->individualCost(i),
            ucb->ucbValue(ucb->reward(i), totalCost, ucb->individualCost(i)));
    }
}
void runSearch(State& state) {
    switch (selectionStrategyChoice) {
        case RANDOM: {
            constructStratAndRun(state, make_shared<RandomNeighbourhood>());
            return;
        }
        case UCB: {
            auto ucb = make_shared<UcbWithMinorNodeCount>(
                state, DEFAULT_UCB_EXPLORATION_BIAS);
            constructStratAndRun(state, ucb);
            saveUcbResults(state, ucb);
            return;
        }
        case UCB_WITH_TRIGGER: {
            auto ucb = make_shared<UcbWithTriggerCount>(
                state, DEFAULT_UCB_EXPLORATION_BIAS);
            constructStratAndRun(state, ucb);
            saveUcbResults(state, ucb);
            return;
        }

        case INTERACTIVE: {
            constructStratAndRun(
                state, make_shared<InteractiveNeighbourhoodSelector>());
            return;
        }
        default:
            abort();
    }
}

void setSignalsAndHandlers() {
    signal(SIGINT, sigIntHandler);
    signal(SIGVTALRM, sigAlarmHandler);
    signal(SIGALRM, sigAlarmHandler);

    if (cpuTimeLimitFlag) {
        setTimeout(cpuTimeLimitArg.get(), true);
    } else if (realTimeLimitFlag) {
        setTimeout(realTimeLimitArg.get(), false);
    }
}

void printFinalStats(const State& state) {
    std::cout << "\n\n";
    if (saveNeighbourhoodStatsArg) {
        state.stats.printNeighbourhoodStats(saveNeighbourhoodStatsArg.get());
    } else {
        state.stats.printNeighbourhoodStats(cout);
    }
    std::cout << "\n\n";
    cout << state.stats << "\nTrigger event count " << triggerEventCount
         << "\n";

    auto times = state.stats.getTime();
    std::cout << "total real time actually spent in neighbourhoods: "
              << state.totalTimeInNeighbourhoods << std::endl;
    std::cout << "Total CPU time: " << times.first << std::endl;
    std::cout << "Total real time: " << times.second << std::endl;
}

int main(const int argc, const char** argv) {
    cout << "ATHANOR\n";
    if (GIT_REVISION != string("GITDIR-NOTFOUND")) {
        cout << "Git revision: " << GIT_REVISION << endl << endl;
    }
    if (argc == 1) {
        argParser.printAllUsageInfo(cout, argv[0]);
        exit(0);
    }
    try {
        argParser.validateArgs(argc, argv, false);
    } catch (ParseException& e) {
        cerr << "Error: " << e.what() << endl;
        cerr << "Successfully parsed: ";
        argParser.printSuccessfullyParsed(cerr, argv);
        cerr << "\n\n";
        cerr << "For usage information, run " << argv[0] << endl;
        exit(1);
    }

    setSignalsAndHandlers();
    try {
        ParsedModel parsedModel =
            (paramArg) ? parseModelFromJson({paramArg.get(), specArg.get()})
                       : parseModelFromJson({specArg.get()});
        State state(parsedModel.builder->build());
        u_int32_t seed = (seedArg) ? seedArg.get() : random_device()();
        globalRandomGenerator.seed(seed);
        cout << "Using seed: " << seed << endl;
        state.disableVarViolations = disableVioBiasFlag;
        runSearch(state);
        printFinalStats(state);
    } catch (nlohmann::detail::exception& e) {
        cerr << "Error parsing JSON: " << e.what() << endl;
        exit(1);
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

volatile bool sigIntActivated = false, sigAlarmActivated = false;
void forceExit() {
    cout << "\n\nFORCE EXIT\n";
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
