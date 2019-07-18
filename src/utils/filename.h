
#ifndef SRC_UTILS_FILENAME_H_
#define SRC_UTILS_FILENAME_H_

#include <string.h>

#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif /* SRC_UTILS_FILENAME_H_ */
