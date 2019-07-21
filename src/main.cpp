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
        "conjure, athanor will first check the flag --conjurePath (see "
        "--conjurePath for details).  If this flag is not set, athanor will "
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
            "--conjurePath", Policy::OPTIONAL,
            "Specify a path to the conjure executable.  Conjure is used to "
            "translate essence files into a JSON representation.  If this flag "
            "is not set, athanor will first look for conjure in the executable "
            "directory and then in the path environment.  However, if "
            "--conjurePath does not point to conjure, an error will be "
            "reported and athanor will exit.")
        .add<Arg<string>>("path_to_executable", Policy::MANDATORY, "");

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
bool repeatSanityCheckOfConst = false;
auto& repeatSanityCheckFlag = sanityCheckFlag.add<Flag>(
    "--repeatCheckOfConst", Policy::OPTIONAL,
    "repeat the checks of expressions that are constant.",
    [](auto&) { repeatSanityCheckOfConst = true; });

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
bool hasIterationLimit = false;
u_int64_t iterationLimit = 0;
auto& iterationLimitFlag = argParser.add<ComplexFlag>(
    "--iterationLimit", Policy::OPTIONAL,
    "Specify the maximum number of iterations to spend in search.");

auto& iterationLimitArg = iterationLimitFlag.add<Arg<u_int64_t>>(
    "number_iterations", Policy::MANDATORY, "0 = no limit", [](string& arg) {
        try {
            iterationLimit = stol(arg);
            hasIterationLimit = true;
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

extern bool quietMode;
bool quietMode = false;
auto& quietModeFlag = argParser.add<Flag>(
    "--quietMode", Policy::OPTIONAL,
    "Do not print stats information every time an improvement to the objective "
    "or violation is made.  If enabled, during search, the output consists "
    "solely of feasible solutions.  Note that stats are still printed once at "
    "the end of search.",
    [](auto&) { quietMode = true; });

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
    cout << "\n\n";
    if (saveNeighbourhoodStatsArg) {
        state.stats.printNeighbourhoodStats(saveNeighbourhoodStatsArg.get());
    } else {
        state.stats.printNeighbourhoodStats(cout);
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
            cerr << toString("Error: the path specified by --conjurePath ('",
                             conjurePathArg.get(),
                             "') does not point to an executable.");
            exit(1);
        }
    }

    string localConjure = getDirectoryContainingExecutable() + "/conjure";
    if (runCommand(localConjure).first == 0) {
        return localConjure;
    } else if (runCommand("conjure").first == 0) {
        return "conjure";
    } else {
        cerr << "Error: conjure not found next to athanor executable directory "
                "nor found in path.\n";
        exit(1);
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
            cerr << "Error, spec did not type check.\n" << result.second;
            exit(1);
        }

        result =
            runCommand(conjurePath, "pretty", file, "--output-format", "json");
        if (result.first == 0) {
            return nlohmann::json::parse(
                find(result.second.begin(), result.second.end(), '{'),
                result.second.end());
        } else {
            cerr << "Error translating essence: " + result.second << endl;
            exit(1);
        }
    } else {
        ifstream is(file);
        nlohmann::json json;
        is >> json;
        return json;
    }
}

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

    try {
        // parse files
        vector<nlohmann::json> jsons = getInputs();
        ParsedModel parsedModel = parseModelFromJson(jsons);
        State state(parsedModel.builder->build());
        u_int32_t seed = (seedArg) ? seedArg.get() : random_device()();
        globalRandomGenerator.seed(seed);
        cout << "Using seed: " << seed << endl;
        state.disableVarViolations = disableVioBiasFlag;
        setSignalsAndHandlers();

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
static const int DELAYED_FORCED_EXIT_TIME = 10;
void forceExit() {
    cout << "\n\nFORCE EXIT\n";
    abort();
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
