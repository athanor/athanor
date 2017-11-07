
#ifndef SRC_TYPES_TYPETRATES_H_
#define SRC_TYPES_TYPETRATES_H_
#include "types/forwardDecls/typesAndDomains.h"

template <typename T>
struct AssociatedDomain;
template <typename T>
struct AssociatedValueType;

template <typename T>
struct IsDomainPtrType : public std::false_type {};
template <typename T>
struct IsDomainType : public std::false_type {};
template <typename T>
struct IsValueType : public std::false_type {};

template <bool, typename>
struct ValRefOrPtrSwitch;

template <typename T>
struct ValRefOrPtrSwitch<true, T> {
    typedef ValRef<T> type;
};
template <typename T>
struct ValRefOrPtrSwitch<false, T> {
    typedef std::shared_ptr<T> type;
};

template <typename T>
struct ValRefOrPtr : ValRefOrPtrSwitch<IsValueType<T>::value, T> {
    // using PtrSwitch<IsValueType<T>::value, T>::type;
};
template <typename T>
struct TypeAsString;
#define makeAssociations(name)                                    \
    template <>                                                   \
    struct AssociatedDomain<name##Value> {                        \
        typedef name##Domain type;                                \
    };                                                            \
                                                                  \
    template <>                                                   \
    struct AssociatedValueType<name##Domain> {                    \
        typedef name##Value type;                                 \
    };                                                            \
    template <>                                                   \
    struct TypeAsString<name##Value> {                            \
        static const std::string value;                           \
    };                                                            \
    template <>                                                   \
    struct TypeAsString<name##Domain> {                           \
        static const std::string value;                           \
    };                                                            \
                                                                  \
    template <>                                                   \
    struct IsDomainPtrType<std::shared_ptr<name##Domain>>         \
        : public std::true_type {};                               \
                                                                  \
    template <>                                                   \
    struct IsDomainType<name##Domain> : public std::true_type {}; \
    template <>                                                   \
    struct IsValueType<name##Value> : public std::true_type {};

buildForAllTypes(makeAssociations, )
#undef makeAssociations

#endif /* SRC_TYPES_TYPETRATES_H_ */
