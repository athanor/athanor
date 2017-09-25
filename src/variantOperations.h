#ifndef SRC_VARIANTOPERATIONS_H_
#define SRC_VARIANTOPERATIONS_H_
#include <type_traits>
#include "variant.hpp"

template <class... Fs>
struct OverLoaded;

template <class F1>
struct OverLoaded<F1> : F1 {
    using F1::operator();

    OverLoaded(F1&& head) : F1(std::forward<F1>(head)) {}
};

template <class F1, class... Fs>
struct OverLoaded<F1, Fs...> : F1, OverLoaded<Fs...> {
    using F1::operator();
    using OverLoaded<Fs...>::operator();

    OverLoaded(F1&& head, Fs&&... rest)
        : F1(std::forward<F1>(head)),
          OverLoaded<Fs...>(std::forward<Fs>(rest)...) {}
};

template <class... Fs>
OverLoaded<Fs...> overloaded(Fs&&... fs) {
    return OverLoaded<Fs...>(std::forward<Fs>(fs)...);
}

template <class T>
struct AlwaysFalse : std::false_type {};

#define variantMatch(variant_type, variant_value)                 \
    constexpr(std::is_same<std::decay_t<decltype(variant_value)>, \
                           variant_type>::value)

#define nonExhaustiveError(variant_value)                                    \
    static_assert(AlwaysFalse<std::decay_t<decltype(variant_value)>>::value, \
                  "non-exhaustive visitor!");

#endif /* SRC_VARIANTOPERATIONS_H_ */
