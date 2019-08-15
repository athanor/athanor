
#ifndef SRC_UTILS_IGNOREUNUSED_H_
#define SRC_UTILS_IGNOREUNUSED_H_
#include "base/intSize.h"
#include "utils/filename.h"
template <typename... Types>
void ignoreUnused(Types&&...) {}

#define todoImpl(...)                                          \
    ignoreUnused(__VA_ARGS__);                                 \
    myCerr << "Error, this function yet to be implemented.\n"; \
    assert(false);                                             \
    myAbort();

#define shouldNotBeCalledPanic                                                 \
    myCerr << "This function should never be called.\n"                        \
           << __func__ << "\nFile: " << __FILENAME__ << "\nLine: " << __LINE__ \
           << "\n";                                                            \
    myAbort();

#endif /* SRC_UTILS_IGNOREUNUSED_H_ */
