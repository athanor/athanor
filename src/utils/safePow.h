
#ifndef SRC_UTILS_SAFEPOW_H_
#define SRC_UTILS_SAFEPOW_H_
#include <optional.hpp>
#include "base/intSize.h"
template <typename T, typename U, typename V>
std::experimental::optional<T> safePow(U a, V b, UInt maxNumberAllowed) {
    if (b * log(std::abs(a)) < log(maxNumberAllowed)) {
        return static_cast<V>(pow(a, b));
    } else {
        return std::experimental::nullopt;
    }
}

#endif /* SRC_UTILS_SAFEPOW_H_ */
