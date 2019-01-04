#ifndef SRC_UTILS_HASHUTILS_H_
#define SRC_UTILS_HASHUTILS_H_
#include <iostream>
u_int64_t mix(char* input, size_t inputSize);

template <typename T>
u_int64_t mix(const T& item) {
    return mix(((char*)&item), sizeof(T));
}

#endif /* SRC_UTILS_HASHUTILS_H_*/
