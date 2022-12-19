#ifndef SRC_UTILS_HASHUTILS_H_
#define SRC_UTILS_HASHUTILS_H_
#include <iostream>

#include "base/intSize.h"
class HashType;

namespace std {
template <>
struct hash<HashType>;
}

class HashType {
    friend std::hash<HashType>;
    UInt64 value = 0;

   public:
    HashType() {}
    explicit HashType(UInt64 value);

    HashType operator+(const HashType other) const;

    HashType operator-(const HashType other) const;
    HashType& operator+=(const HashType other);
    HashType& operator-=(const HashType other);
    HashType operator*(size_t repeatAmount) const;
    friend HashType operator*(size_t repeatAmount, HashType hash);
    bool operator==(const HashType& other) const;
    bool operator!=(const HashType& other) const;
    bool operator<(const HashType& other) const;
    bool operator<=(const HashType& other) const;
    friend std::ostream& operator<<(std::ostream& os, const HashType& val);
    friend HashType mix(const HashType& hash);
};

namespace std {
template <>
struct hash<HashType> {
    size_t operator()(const HashType& val) const;
};
}  // namespace std

HashType mix(char* input, size_t inputSize);

#endif /* SRC_UTILS_HASHUTILS_H_*/
