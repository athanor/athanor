
#ifndef COMMON_TUPLEFOREACH_H
#define COMMON_TUPLEFOREACH_H

template <std::size_t I = 0, typename FuncT, typename... Tp>
inline typename std::enable_if<I == sizeof...(Tp), bool>::type forEach(
    const std::tuple<Tp...> &, const FuncT &) {
    return true;
}

template <std::size_t I = 0, typename FuncT, typename... Tp>
    inline typename std::enable_if <
    I<sizeof...(Tp), bool>::type forEach(const std::tuple<Tp...> &t,
                                         const FuncT &f) {
    if (!f(std::get<I>(t))) {
        return false;
    }
    return forEach<I + 1, FuncT, Tp...>(t, f);
}

#endif /* COMMON_TUPLEFOREACH_H */
