
#ifndef SRC_UTILS_RUNCOMMAND_H_
#define SRC_UTILS_RUNCOMMAND_H_
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
namespace RunCommandDetail {
inline char* toStr(char* str) { return str; }
inline const char* toStr(const std::string& str) { return str.data(); }
}  // namespace RunCommandDetail
template <typename... Args>
std::pair<int, std::string> runCommand(const std::string& command,
                                       const Args&... args) {
    pid_t pid = 0;
    std::array<int, 2> pipeFD;
    pipe(pipeFD.data());
    pid = fork();
    if (pid == 0) {
        // child
        close(pipeFD[0]);
        dup2(pipeFD[1], STDOUT_FILENO);
        dup2(pipeFD[1], STDERR_FILENO);
        execlp(command.data(), command.data(), RunCommandDetail::toStr(args)...,
               (char*)NULL);
        exit(1);
    }
    // Only parent gets here
    close(pipeFD[1]);
    // file* with auto fclose
    std::unique_ptr<FILE, decltype(&fclose)> outputStream(
        fdopen(pipeFD[0], "r"), fclose);
    std::array<char, 256> line;
    std::string output;
    while (fgets(line.data(), line.size(), outputStream.get())) {
        output += line.data();
    }
    int status;
    waitpid(pid, &status, 0);
    return std::make_pair(status, output);
}

#endif /* SRC_UTILS_RUNCOMMAND_H_ */
