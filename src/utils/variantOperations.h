#ifndef SRC_VARIANTOPERATIONS_H_
#define SRC_VARIANTOPERATIONS_H_
#include <mpark/variant.hpp>
#include <type_traits>

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

template <bool, typename Func>
struct StaticIf;
template <bool cond, typename Func>
auto staticIf(Func&& func) {
    return StaticIf<cond, Func>(std::forward<Func>(func));
}

template <typename Func>
struct StaticIf<true, Func> : public Func {
    StaticIf(Func&& func) : Func(std::forward<Func>(func)) {}
    template <typename Func2>
    inline StaticIf<true, Func> otherwise(Func2&&) {
        return std::move(*this);
    }

    template <bool, typename Func2>
    inline StaticIf<true, Func> elseIf(Func2&&) {
        return std::move(*this);
    }
};

template <typename Func>
struct StaticIf<false, Func> {
    StaticIf(Func&&) {}
    template <typename Func2>
    inline decltype(auto) otherwise(Func2&& func) {
        return std::forward<Func2>(func);
    }
    template <bool condition, typename Func2>
    inline decltype(auto) elseIf(Func2&& func) {
        return staticIf<condition>(std::forward<Func2>(func));
    }
};

/*
template <class T>
struct AlwaysFalse : std::false_type {};

#define variantMatch(variant_type, variant_value)                 \
    constexpr(std::is_same<std::decay_t<decltype(variant_value)>, \
                           variant_type>::value)

#define nonExhaustiveError(variant_value)                                    \
    static_assert(AlwaysFalse<std::decay_t<decltype(variant_value)>>::value, \
                  "non-exhaustive visitor!");
*/

#endif /* SRC_VARIANTOPERATIONS_H_ */
