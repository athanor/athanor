#include "utils/hashUtils.h"
#include <iostream>
#include <utility>
#include "MurmurHash3.h"
#include "base/intSize.h"
#include "picosha2.h"

HashType::HashType(u_int64_t value) : value(value) {}

HashType HashType::operator+(const HashType other) const {
    return HashType(value + other.value);
}

HashType HashType::operator-(const HashType other) const {
    return HashType(value - other.value);
}

HashType& HashType::operator+=(const HashType other) {
    value += other.value;
    return *this;
}
HashType& HashType::operator-=(const HashType other) {
    value -= other.value;
    return *this;
}
HashType HashType::operator*(size_t repeatAmount) const {
    return HashType(value * repeatAmount);
}

HashType operator*(size_t repeatAmount, HashType hash) {
    return hash * repeatAmount;
}

bool HashType::operator==(const HashType& other) const {
    return value == other.value;
}
bool HashType::operator!=(const HashType& other) const {
    return value != other.value;
}

std::ostream& operator<<(std::ostream& os, const HashType& val) {
    return os << val.value;
}

size_t std::hash<HashType>::operator()(const HashType& val) const {
    return val.value;
}

u_int64_t truncatedSha256(char* input, size_t inputSize) {
    std::vector<unsigned char> hash(8);
    picosha2::hash256(input, input + inputSize, hash);
    u_int64_t truncatedHash = 0;
    for (int i = 0; i < 8; ++i) {
        u_int64_t temp = hash[i];
        truncatedHash |= (temp << i * 8);
    }
    return truncatedHash;
}

u_int64_t truncatedMurmurHash3(char* input, size_t inputSize) {
    static const unsigned int MURMURHASH3_SEED = 0;
    u_int64_t result[2];
    MurmurHash3_x64_128(input, inputSize, MURMURHASH3_SEED, result);
    return result[0] ^ result[1];
}

HashType mix(char* input, size_t inputSize) {
    return HashType(truncatedMurmurHash3(input, inputSize));
}

HashType mix(const HashType& val) {
    return mix(((char*)&(val.value)), sizeof(HashType));
}
