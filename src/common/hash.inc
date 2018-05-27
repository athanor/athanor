template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
namespace std {
template <typename T1, typename T2>
class hash<pair<T1, T2> > {
   public:
    size_t operator()(const pair<T1, T2>& pairIn) const {
        size_t seed = 13;
        hash_combine(seed, hash<T1>()(pairIn.first));
        hash_combine(seed, hash<T2>()(pairIn.second));
        return seed;
    }
};

template <typename... Ty>
class hash<tuple<Ty...> > {
   public:
    size_t operator()(const tuple<Ty...>& tupleIn) const {
        size_t seed = 13;
        forEach(tupleIn, [&](auto& v) {
            hash_combine(
                seed,
                hash<typename remove_const<
                    typename remove_reference<decltype(v)>::type>::type>()(v));
            return true;
        });
        return seed;
    }
};
}  // namespace std
