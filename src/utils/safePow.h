
#ifndef SRC_UTILS_SAFEPOW_H_
#define SRC_UTILS_SAFEPOW_H_
#include <optional.hpp>
#include "base/intSize.h"

template <typename T,
          typename std::enable_if<std::is_signed<T>::value, int>::type = 0>
T myAbs(T v) {
    return std::abs(v);
}
template <typename T,
          typename std::enable_if<!std::is_signed<T>::value, int>::type = 0>
T myAbs(T v) {
    return v;
}

template <typename T, typename U, typename V, typename W>
std::experimental::optional<T> safePow(U a, V b, W maxNumberAllowed) {
    if (b * log(myAbs(a)) < log(myAbs(maxNumberAllowed))) {
        return static_cast<V>(pow(a, b));
    } else {
        return std::experimental::nullopt;
    }
}

#endif /* SRC_UTILS_SAFEPOW_H_ */
