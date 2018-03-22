#include <autoArgParse/argParser.h>
#include <sys/time.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "common/common.h"
#include "jsonModelParser.h"
#include "search/searchStrategies.h"
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
auto& randomSeedFlag = argParser.add<ComplexFlag>(
    "--randomSeed", Policy::OPTIONAL, "Specify a random seed.",
    [](const string&) { useSeedForRandom = true; });
auto& seedArg = randomSeedFlag.add<Arg<int>>(
    "integer_seed", Policy::MANDATORY,
    "Integer seed to use for random generator.",
    chain(Converter<int>(), [](int value) {
        if (value < 0) {
            throw ErrorMessage("Seed must be greater or equal to 0.");
        } else {
            seedForRandom = (u_int64_t)(value);
            return value;
        }
    }));
void testHashes() {
    const size_t max = 10000;
    unordered_map<u_int64_t, pair<u_int64_t, u_int64_t>> seenHashes;
    for (u_int64_t i = 0; i < max; ++i) {
        for (u_int64_t j = i; j < max; ++j) {
            u_int64_t hashSum = mix(i) + mix(j);
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

void setTimeout(int numberSeconds, bool virtualTimer);
void sigIntHandler(int);
void sigAlarmHandler(int);

int main(const int argc, const char** argv) {
    argParser.validateArgs(argc, argv);
    ParsedModel parsedModel = parseModelFromJson(fileArg.get());
    Model model = parsedModel.builder->build();
    auto search = HillClimber<RandomNeighbourhoodWithViolation>(move(model));
    signal(SIGINT, sigIntHandler);
    signal(SIGVTALRM, sigAlarmHandler);
    signal(SIGALRM, sigAlarmHandler);

    if (cpuTimeLimitFlag) {
        setTimeout(cpuTimeLimitArg.get(), true);
    } else if (realTimeLimitFlag) {
        setTimeout(realTimeLimitArg.get(), false);
    }
    search.search();
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
void sigIntHandler(int) { sigIntActivated = true; }
void sigAlarmHandler(int) { sigAlarmActivated = true; }
