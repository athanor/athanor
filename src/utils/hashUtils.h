#ifndef SRC_UTILS_HASHUTILS_H_
#define SRC_UTILS_HASHUTILS_H_
#include <iostream>
#include <utility>
#include "picosha2.h"
#include "utils/fnv/fnv.h"
#include "utils/murmurHash/MurmurHash3.h"
// default values recommended by http://isthe.com/chongo/tech/comp/fnv/

static const uint32_t FNV1A_PRIME = 0x01000193;  //   16777619
static const uint32_t FNV1A_SEED = 0x811C9DC5;   // 2166136261
inline uint64_t fnv1a(unsigned char oneByte, uint64_t hash = FNV1A_SEED) {
    return (oneByte ^ hash) * FNV1A_PRIME;
}
template <typename T>
inline u_int64_t mix_single(const T& value, uint64_t hash = FNV1A_SEED) {
    const unsigned char* ptr = (const unsigned char*)&value;
    for (size_t i = 0; i < sizeof(T); ++i) {
        hash = fnv1a(*ptr++, hash);
    }
    return hash;
}

template <typename T>
inline u_int64_t truncatedSha256(const T& item) {
    std::vector<unsigned char> hash(8);
    picosha2::hash256((char*)(&item), ((char*)(&item)) + sizeof(T), hash);
    u_int64_t truncatedHash = 0;
    for (int i = 0; i < 8; ++i) {
        u_int64_t temp = hash[i];
        truncatedHash |= (temp << i * 8);
    }
    return truncatedHash;
}

template <typename T>
inline u_int64_t mix(const T& arg) {
    //    uint64_t hash = FNV1A_SEED;
    //    // hack to apply mix_single to all types
    //
    //    int unpack[]{0, (hash = mix_single(args, hash), 0)...};
    //    static_cast<void>(unpack);
    //    return hash;

    // return fnv_64a_buf((void*)(hashes), sizeof(hashes), FNV1a_64);
    //    return truncatedSha256(arg);
    u_int64_t result[2];
    MurmurHash3_x64_128(((void*)&arg), sizeof(T), 0, result);
    return result[0] ^ result[1];
}

#endif /* SRC_UTILS_HASHUTILS_H_ */
