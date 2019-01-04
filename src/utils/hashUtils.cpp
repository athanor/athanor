#include "utils/hashUtils.h"
#include <iostream>
#include <utility>
#include "MurmurHash3.h"
#include "picosha2.h"
// default values recommended by http://isthe.com/chongo/tech/comp/fnv/

inline u_int64_t truncatedSha256(char* input, size_t inputSize) {
    std::vector<unsigned char> hash(8);
    picosha2::hash256(input, input + inputSize, hash);
    u_int64_t truncatedHash = 0;
    for (int i = 0; i < 8; ++i) {
        u_int64_t temp = hash[i];
        truncatedHash |= (temp << i * 8);
    }
    return truncatedHash;
}

inline u_int64_t truncatedMurmurHash3(char* input, size_t inputSize) {
    static const unsigned int MURMURHASH3_SEED = 0;
    u_int64_t result[2];
    MurmurHash3_x64_128(input, inputSize, MURMURHASH3_SEED, result);
    return result[0] ^ result[1];
}

u_int64_t mix(char* input, size_t inputSize) {
    return truncatedMurmurHash3(input, inputSize);
}
