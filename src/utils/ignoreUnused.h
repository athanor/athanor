
#ifndef SRC_UTILS_IGNOREUNUSED_H_
#define SRC_UTILS_IGNOREUNUSED_H_
#include "utils/filename.h"
template <typename... Types>
void ignoreUnused(Types&&...) {}

#define todoImpl(...)                                             \
    ignoreUnused(__VA_ARGS__);                                    \
    std::cerr << "Error, this function yet to be implemented.\n"; \
    assert(false);                                                \
    abort();

#define shouldNotBeCalledPanic                             \
    std::cerr << "This function should never be called.\n" \
              << __func__ << "\nFile: " << __FILENAME__    \
              << "\nLine: " << __LINE__ << "\n";           \
    abort();

#endif /* SRC_UTILS_IGNOREUNUSED_H_ */
