#include "utils/getExecPath.h"
#include <libgen.h>
#include <iostream>
#include <string>
#include <vector>
#include "utils/ignoreUnused.h"
#ifndef WASM_TARGET
#ifdef __linux__
#include <limits.h>
#include <unistd.h>
#elif __APPLE__
#include <mach-o/dyld.h>
#endif

/*
 * Returns the full path to the currently running executable,
 * or an empty string in case of failure.
 */
std::string getExecutablePath() {
#ifdef __linux__
    char exePath[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
    if (len == -1 || len == sizeof(exePath)) len = 0;
    exePath[len] = '\0';
#elif __APPLE__
    char exePath[PATH_MAX];
    uint32_t len = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &len) != 0) {
        exePath[0] = '\0';  // buffer too small (!)
    } else {
        // resolve symlinks, ., .. if possible
        char *canonicalPath = realpath(exePath, NULL);
        if (canonicalPath != NULL) {
            strncpy(exePath, canonicalPath, len);
            free(canonicalPath);
        }
    }
#endif
    return std::string(exePath);
}
#else
std::string getExecutablePath() {
    myCerr << "not allowed in WASM\n";
    shouldNotBeCalledPanic;
}
#endif

std::string getDirectoryContainingExecutable() {
    std::string execPath = getExecutablePath();
    std::vector<char> cStr(execPath.c_str(),
                           execPath.c_str() + execPath.size() + 1);
    return std::string(dirname(cStr.data()));
}
