#include <autoArgParse/argParser.h>
#include <sys/time.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include "common/common.h"
#include "gitRevision.h"
#include "parsing/jsonModelParser.h"
#include "search/neighbourhoodSelectionStrategies.h"
#include "search/searchStrategies.h"
#include "search/solver.h"
#include "utils/getExecPath.h"
#include "utils/hashUtils.h"
#include "utils/runCommand.h"
#ifdef WASM_TARGET
#include <emscripten/bind.h>
#endif
using namespace std;
using namespace AutoArgParse;

ArgParser argParser;

string makeMessageOnFiles(const bool essenceOrParam) {
    const char* ext = (essenceOrParam) ? ".essence" : ".param";
    const char* description =
        (essenceOrParam) ? "essence" : "essence parameter";
    return toString(
        "If an ", description, " file is given (denoted by the file extension ",
        ext,
        "), the tool conjure will be used to translate the file into a JSON "
        "representation before parsing.  Otherwise, the file will be "
        "treated as a JSON file and parsed directly.  When searching for "
        "conjure, athanor will first check the flag --conjure-path (see "
        "--conjure-path for details).  If this flag is not set, athanor will "
        "check if conjure can be found in the same directory as the athanor "
        "executable. If not found, the path environment variable will be "
        "used.");
}

auto& specFlag = argParser.add<ComplexFlag>(
    "--spec", Policy::MANDATORY, "Precedes essence specification file.");
auto& specArg = specFlag.add<Arg<string>>(
    "path_to_file", Policy::MANDATORY,
    string("path to an essence specification.  ") + makeMessageOnFiles(true),
    [](const string& path) -> string {
        ifstream file;
        file.open(path);
        if (file.good()) {
            return path;
        } else {
            throw ErrorMessage("Error opening file: " + path);
        }
    });

auto& paramFlag = argParser.add<ComplexFlag>("--param", Policy::OPTIONAL,
                                             "Precedes parameter file.");
auto& paramArg = paramFlag.add<Arg<string>>(
    "path_to_file", Policy::MANDATORY,
    string("path to a parameter file.  ") + makeMessageOnFiles(false),
    [](const string& path) -> string {
        ifstream file;
        file.open(path);
        if (file.good()) {
            return path;
        } else {
            throw ErrorMessage("Error opening file: " + path);
        }
    });
auto& conjurePathArg =
    argParser
        .add<ComplexFlag>(
            "--conjure-path", Policy::OPTIONAL,
            "Specify a path to the conjure executable.  Conjure is used to "
            "translate essence files into a JSON representation.  If this flag "
            "is not set, athanor will first look for conjure in the executable "
            "directory and then in the path environment.  However, if "
            "--conjure-path does not point to conjure, an error will be "
            "reported and athanor will exit.")
        .add<Arg<string>>("path_to_conjure_executable", Policy::MANDATORY, "");

extern string bestSolution;
extern bool saveBestSolution;
string bestSolution;
bool saveBestSolution = false;

auto& saveBestSolutionFlag = argParser.add<ComplexFlag>(
    "--save-best-solution", Policy::OPTIONAL,
    "Saves the best solution to the specified file.  Note, if the process is "
    "terminated with a non-interruptible signal (SIGSTOP, SIGKILL, ...), the "
    "file may not be created.  Solutions are always printed to STDOUT though. "
    " A clean exit, for example after a solver time out, iteration limit or on "
    "receiving Control-C  will still result in the best solution being "
    "correctly written to the specified file.",
    [](auto&) { saveBestSolution = true; });
auto& bestSolutionFileArg =
    saveBestSolutionFlag.add<Arg<ofstream>>("file_path", Policy::MANDATORY, "");

mt19937 globalRandomGenerator;
auto& randomSeedFlag = argParser.add<ComplexFlag>(
    "--random-seed", Policy::OPTIONAL, "Specify a random seed.");
auto& seedArg = randomSeedFlag.add<Arg<unsigned int>>(
    "integer_seed", Policy::MANDATORY,
    "Integer seed to use for random generator.");

auto& exclusiveTimeLimitGroup = argParser.makeExclusiveGroup(Policy::OPTIONAL);
auto& cpuTimeLimitFlag = exclusiveTimeLimitGroup.add<ComplexFlag>(
    "--cpu-time-limit", "Specify the CPU time limit in seconds.");

auto& cpuTimeLimitArg =
    cpuTimeLimitFlag.add<Arg<int>>("number_seconds", Policy::MANDATORY, "");

auto& realTimeLimitFlag = exclusiveTimeLimitGroup.add<ComplexFlag>(
    "--real-time-limit", "Specify the CPU time limit in seconds.");

auto& realTimeLimitArg =
    realTimeLimitFlag.add<Arg<int>>("number_seconds", Policy::MANDATORY, "");
bool hasIterationLimit = false;
UInt64 iterationLimit = 0;
auto& iterationLimitFlag = argParser.add<ComplexFlag>(
    "--iteration-limit", Policy::OPTIONAL,
    "Specify the maximum number of iterations to spend in search.");

auto& iterationLimitArg = iterationLimitFlag.add<Arg<UInt64>>(
    "number_iterations", Policy::MANDATORY, "",
    chain(Converter<UInt64>(), [](UInt64 value) {
        hasIterationLimit = true;
        iterationLimit = value;
        return value;
    }));

bool hasSolutionLimit = false;
UInt64 solutionLimit = 0;
auto& solutionLimitFlag = argParser.add<ComplexFlag>(
    "--solution-limit", Policy::OPTIONAL,
    "Exit search if the specified number of solutions has been found.  Note, "
    "this only applies to optimisation problems.  Only solutions that improve "
    "on the objective are counted.");

auto& solutionLimitArg = solutionLimitFlag.add<Arg<UInt64>>(
    "number_solutions", Policy::MANDATORY, "Value greater 0",
    chain(Converter<UInt64>(), [](UInt64 value) {
        if (value < 1) {
            throw ErrorMessage("Value must be greater than 0.");
        }
        hasSolutionLimit = true;
        solutionLimit = value;
        return value;
    }));

extern bool noPrintSolutions;
bool noPrintSolutions = false;
auto& noPrintSolutionsFlag = argParser.add<Flag>(
    "--no-print-solutions", Policy::OPTIONAL,
    "Do not print solutions, useful for timing experiements.",
    [](auto&) { noPrintSolutions = true; });

extern bool quietMode;
bool quietMode = true;
auto& verboseModeFlag = argParser.add<Flag>(
    "--show-progress-stats", Policy::OPTIONAL,
    "print stats information every time an improvement to the objective "
    "or violation is made.",
    [](auto&) { quietMode = false; });

auto& showNhStatsFlag = argParser.add<ComplexFlag>(
    "--show-nh-stats", Policy::OPTIONAL,
    "Print stats specific to each neighbourhood in a CSV format to stdout.");

auto& showNhStatsArg = showNhStatsFlag.add<Arg<ofstream>>(
    "path_to_file", Policy::OPTIONAL,
    "If specified, write the neighbourhood stats into a file instead of "
    "stdout.");

auto& saveUcbArg =
    argParser
        .add<ComplexFlag>("--save-ucb-state", Policy::OPTIONAL,
                          "Save learned neighbourhood UCB info.\n")
        .add<Arg<ofstream>>("path_to_file", Policy::MANDATORY,
                            "File to save results to.");

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

auto& disableVioBiasFlag = argParser.add<Flag>(
    "--disable-vio-bias", Policy::OPTIONAL,
    "Disable the search from biasing towards violating variables.");

extern bool allowForwardingOfDefiningExprs;
bool allowForwardingOfDefiningExprs = true;
auto& disableDefinedExprsFlag =
    argParser.add<Flag>("--disable-defined-vars", Policy::OPTIONAL,
                        "Disable the forwarding of values from expressions to "
                        "the variables that they define through equality.  "
                        "This does not include top level equalities.",
                        [](auto&) { allowForwardingOfDefiningExprs = false; });
void setTimeout(int numberSeconds, bool virtualTimer);
void sigIntHandler(int);
void sigAlarmHandler(int);

bool runSanityChecks = false;
bool verboseSanityError = false;
auto& sanityCheckFlag = argParser.add<ComplexFlag>(
    "--sanity-check", Policy::OPTIONAL,
    "Activate sanity check mode, this is a debugging feature,.  After each "
    "move, the state is scanned for errors caused by bad triggering.  Note, "
    "errors caused by hash collisions are not tested for in this mode.",
    [](auto&) { runSanityChecks = true; });

auto& verboseErrorFlag = sanityCheckFlag.add<Flag>(
    "--verbose", Policy::OPTIONAL, "Verbose printing at point of error.",
    [](auto&) { verboseSanityError = true; });
bool repeatSanityCheckOfConst = false;
auto& repeatSanityCheckFlag = sanityCheckFlag.add<Flag>(
    "--repeat-check-of-const", Policy::OPTIONAL,
    "repeat the checks of expressions that are constant.",
    [](auto&) { repeatSanityCheckOfConst = true; });

bool dontSkipSanityCheckForAlreadyVisitedChildren = false;
auto& dontSkipSanityCheckFlag = sanityCheckFlag.add<Flag>(
    "--dont-skip-repeat-visits", Policy::OPTIONAL,
    "When a child has multiple parents, the child will be visited multiple "
    "times during sanity checking.  Only the first visit causes a sanity check "
    "of child unless this option is enabled.",
    [](auto&) { dontSkipSanityCheckForAlreadyVisitedChildren = true; });

extern UInt64 sanityCheckInterval;
UInt64 sanityCheckInterval = 1;
auto& sanityCheckIntervalArg =
    sanityCheckFlag
        .add<ComplexFlag>("--at-intervals-of", Policy::OPTIONAL,
                          "Only run sanity check once per the specified number "
                          "of iterations.")
        .add<Arg<UInt64>>("interval_size", Policy::MANDATORY, "",
                          chain(Converter<UInt64>(), [](UInt64 value) {
                              sanityCheckInterval = value;
                              return value;
                          }));

debug_code(bool debugLogAllowed = true;
           auto& disableDebugLoggingFlag = argParser.add<Flag>(
               "--disable-debug-log", Policy::OPTIONAL,
               "Included only for debug builds, can be used to silence "
               "logging "
               "but keeping assertions switched on.",
               [](auto&) { debugLogAllowed = false; }););

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
            myAbort();
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
            myAbort();
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
            myAbort();
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
    if (showNhStatsFlag) {
        if (showNhStatsArg) {
            state.stats.printNeighbourhoodStats(showNhStatsArg.get());
        } else {
            state.stats.printNeighbourhoodStats(cout << "\n\n");
        }
    }
    cout << "\n\n";
    cout << state.stats << "\nTrigger event count " << triggerEventCount
         << "\n";

    auto times = state.stats.getTime();
    cout << "total real time actually spent in neighbourhoods: "
         << state.totalTimeInNeighbourhoods << endl;
    cout << "Total CPU time: " << times.first << endl;
    cout << "Total real time: " << times.second << endl;
}

string findConjure() {
    if (conjurePathArg) {
        if (runCommand(conjurePathArg.get()).first == 0) {
            return conjurePathArg.get();
        } else {
            myCerr << toString("Error: the path specified by --conjure-path ('",
                               conjurePathArg.get(),
                               "') does not point to an executable.");
            myExit(1);
        }
    }

    string localConjure = getDirectoryContainingExecutable() + "/conjure";
    if (runCommand(localConjure).first == 0) {
        return localConjure;
    } else if (runCommand("conjure").first == 0) {
        return "conjure";
    } else {
        myCerr
            << "Error: conjure not found next to athanor executable directory "
               "nor found in path.\n";
        myExit(1);
    }
}

bool endsWith(const string& fullString, const string& ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(),
                                        ending.length(), ending));
    } else {
        return false;
    }
}

nlohmann::json parseJson(const string& file, bool useConjure,
                         const string& conjurePath, bool specFile) {
    if (useConjure) {
        cout << "Using conjure to translate "
             << (specFile ? "essence" : "param") << " file\n";
        // first typecheck
        pair<int, string> result;
        if (specFile &&
            (result = runCommand(conjurePath, "type-check", file)).first != 0) {
            myCerr << "Error, spec did not type check.\n" << result.second;
            myExit(1);
        }

        result =
            runCommand(conjurePath, "pretty", file, "--output-format", "json");
        if (result.first == 0) {
            return nlohmann::json::parse(
                find(result.second.begin(), result.second.end(), '{'),
                result.second.end());
        } else {
            myCerr << "Error translating essence: " + result.second << endl;
            myExit(1);
        }
    } else {
        ifstream is(file);
        nlohmann::json json;
        is >> json;
        return json;
    }
}

#ifndef WASM_TARGET
static vector<nlohmann::json> getInputs() {
    string conjurePath;
    chrono::high_resolution_clock::time_point startTime =
        chrono::high_resolution_clock::now();
    if (endsWith(specArg.get(), ".essence") ||
        endsWith(paramArg.get(), ".param")) {
        conjurePath = findConjure();
    }
    vector<nlohmann::json> jsons;
    jsons.emplace_back(parseJson(
        specArg.get(), endsWith(specArg.get(), ".essence"), conjurePath, true));
    if (paramArg) {
        jsons.insert(
            jsons.begin(),
            parseJson(paramArg.get(), endsWith(paramArg.get(), ".param"),
                      conjurePath, false));
    }
    chrono::high_resolution_clock::time_point endTime =
        chrono::high_resolution_clock::now();
    cout << "Read time (real): "
         << chrono::duration<double>(endTime - startTime).count() << "s\n";
    return jsons;
}

int main(const int argc, const char** argv) {
    cout << "ATHANOR\n";

    if (GIT_REVISION != string("GITDIR-NOTFOUND")) {
        cout << "Git revision: " << GIT_REVISION << endl << endl;
    }
    if (argc == 1) {
        argParser.printAllUsageInfo(cout, argv[0]);
        myExit(0);
    }
    try {
        argParser.validateArgs(argc, argv, false);
    } catch (ParseException& e) {
        myCerr << "Error: " << e.what() << endl;
        myCerr << "Successfully parsed: ";
        argParser.printSuccessfullyParsed(myCerr, argv);
        myCerr << "\n\n";
        myCerr << "For usage information, run " << argv[0] << endl;
        myExit(1);
    }

    try {
        // parse files
        vector<nlohmann::json> jsons = getInputs();
        ParsedModel parsedModel = parseModelFromJson(jsons);
        State state(parsedModel.builder->build());
        unsigned int seed = (seedArg) ? seedArg.get() : random_device()();
        globalRandomGenerator.seed(seed);
        cout << "Using seed: " << seed << endl;
        state.disableVarViolations = disableVioBiasFlag;
        setSignalsAndHandlers();

        runSearch(state);
        if (saveBestSolution) {
            bestSolutionFileArg.get() << bestSolution;
        }
        printFinalStats(state);
    } catch (nlohmann::detail::exception& e) {
        myCerr << "Error parsing JSON: " << e.what() << endl;
        myExit(1);
    } catch (SanityCheckException& e) {
        myCerr << "***SANITY CHECK ERROR: " << e.errorMessage << endl;
        if (!e.file.empty()) {
            myCerr << "From file: " << e.file << endl;
        }
        if (e.line > 0) {
            myCerr << "Line: " << e.line << endl;
        }
        if (!e.stateDump.empty()) {
            myCerr << "Operator state: " << e.stateDump << endl;
        }
        size_t i = 1;
        for (auto& message : e.messageStack) {
            myCerr << i << ": " << message << endl;
            ++i;
        }

        myAbort();
    }
}
#endif
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
static const int DELAYED_FORCED_EXIT_TIME = 10;
void forceExit() {
    cout << "\n\nFORCE EXIT\n";
    myAbort();
}
void sigIntHandler(int) {
    if (sigIntActivated) {
        forceExit();
    }
    sigIntActivated = true;
    setTimeout(DELAYED_FORCED_EXIT_TIME, false);
}
void sigAlarmHandler(int) {
    if (sigIntActivated || sigAlarmActivated) {
        forceExit();
    }
    sigAlarmActivated = true;
    setTimeout(DELAYED_FORCED_EXIT_TIME, false);
}
#ifndef WASM_TARGET
void signalEndOfSearch() { throw EndOfSearchException(); }
#else
using namespace emscripten;
void signalEndOfSearch() {
    val::global().call<void>("reportEndOfSearch");
    exit(0);
}

/* for running with web assembly */
void runSolverWasm(string specJsonString, string paramJsonString) {
    vector<nlohmann::json> jsons;
    if (!paramJsonString.empty()) {
        jsons.emplace_back(nlohmann::json::parse(
            find(begin(paramJsonString), end(paramJsonString), '{'),
            end(paramJsonString)));
    }
    jsons.emplace_back(
        nlohmann::json::parse(begin(specJsonString), end(specJsonString)));
    ParsedModel parsedModel = parseModelFromJson(jsons);
    State state(parsedModel.builder->build());
    UInt32 seed = (seedArg) ? seedArg.get() : random_device()();
    globalRandomGenerator.seed(seed);
    cout << "Using seed: " << seed << endl;
    runSearch(state);
    printFinalStats(state);
}

std::ostringstream myCerr;
void reportErrorToWebApp(size_t n) {
    string error = myCerr.str();
    if (n != 0 || !error.empty()) {
        val::global().call<void>("reportError", error);
    }
}

EMSCRIPTEN_BINDINGS(my_module) {
    emscripten::function("runSolverWasm", &runSolverWasm);
}
#endif
