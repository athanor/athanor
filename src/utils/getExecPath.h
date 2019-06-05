
#ifndef SRC_UTILS_GETEXECPATH_H_
#define SRC_UTILS_GETEXECPATH_H_
#include <string>
/*
 * Returns the full path to the currently running executable,
 * or an empty string in case of failure.
 */
std::string getExecutablePath();
std::string getDirectoryContainingExecutable();
#endif /* SRC_UTILS_GETEXECPATH_H_ */
