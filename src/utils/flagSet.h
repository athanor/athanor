
#ifndef SRC_UTILS_FLAGSET_H_
#define SRC_UTILS_FLAGSET_H_

#include <array>

template <typename... Flags>
class FlagSet {
    static const int NUMBER_FLAGS = sizeof...(Flags);
    static const int ARRAY_SIZE = NUMBER_FLAGS / 8 + ((NUMBER_FLAGS % 8) != 0);
    std::array<char, ARRAY_SIZE> bits = {};
    // compile time calculation of index of each type (flag)
    template <typename...>
    struct Index;
    // base case, index of type = 0
    template <typename T, typename... Ts>
    struct Index<T, T, Ts...> {
        static const std::size_t value = 0;
    };
    // recursive case
    template <typename T, typename U, typename... Ts>
    struct Index<T, U, Ts...> {
        static const std::size_t value = 1 + Index<T, Ts...>::value;
    };

    /*retrieve value given a array index and a bit index into the array element
     */
    template <int arrayIndex, int bitIndex>
    inline bool value() const {
        const char& flag = bits[arrayIndex];
        return flag & (1 << bitIndex);
    }

    /* Reference object, converts a raw index into an array index and bit index,
     * and provides functions for setting and getting the value of the flag*/
    template <int rawIndex>
    class Reference {
        friend class FlagSet<Flags...>;
        static const int arrayIndex = rawIndex / 8;
        static const int bitIndex = rawIndex % 8;
        FlagSet<Flags...>& flags;

        Reference(FlagSet<Flags...>& flags) : flags(flags) {}

       public:
        inline operator bool() { return flags.value<arrayIndex, bitIndex>(); }
        inline Reference<rawIndex>& operator=(bool set) {
            char& flag = flags.bits[arrayIndex];
            flag &= ~(((char)1) << bitIndex);
            char mask = set;
            mask = (mask << bitIndex);
            flag |= mask;
            return *this;
        }
    };

   public:
    template <typename T>
    inline auto get() {
        return Reference<Index<T, Flags...>::value>(*this);
    }
    template <typename T>
    inline auto get() const {
        typedef Reference<Index<T, Flags...>::value> Ref;
        return value<Ref::arrayIndex, Ref::bitIndex>();
    }

    /* default constructor, zero initialise bits */
    FlagSet() {}
    /*Constructor Initialise from array of booleans*/
    template <typename T, int N>
    FlagSet(const T (&initArray)[N]) : bits() {
        static_assert(std::is_same<bool, T>::value,
                      "If giving a value to each flag, values must be given as "
                      "booleans.");
        static_assert(N == NUMBER_FLAGS,
                      "Not enough values given, a value for each flag is "
                      "required.  Alternatively use the default constructor to "
                      "initialise all flags to false.");
        init<0>(initArray);
    }

   private:
    template <int index,
              typename std::enable_if<index<NUMBER_FLAGS, int>::type = 0> void
                  init(const bool (&initArray)[NUMBER_FLAGS]) {
        if (index < NUMBER_FLAGS) {
            auto r = Reference<index>(*this);
            r = initArray[index];
            init<index + 1>(initArray);
        }
    }
    template <int index,
              typename std::enable_if<index == NUMBER_FLAGS, int>::type = 0>
    void init(const bool (&)[NUMBER_FLAGS]) {}
};

#endif /* SRC_UTILS_FLAGSET_H_ */
