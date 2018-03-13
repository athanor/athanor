
#ifndef SRC_BASE_TYPEDECLS_H_
#define SRC_BASE_TYPEDECLS_H_
#include "base/typesMacros.h"
// forward declare structs
#define declDomainsAndValues(name) \
    struct name##Value;            \
    struct name##View;             \
    struct name##Domain;
buildForAllTypes(declDomainsAndValues, )
#undef declDomainsAndValues

    // associations between values, domains and views

    template <typename T>
    struct AssociatedDomain;
template <typename T>
struct AssociatedValueType;

template <typename T>
struct AssociatedViewType;

template <typename T>
struct IsDomainPtrType : public std::false_type {};
template <typename T>
struct IsDomainType : public std::false_type {};
template <typename T>
struct IsValueType : public std::false_type {};
template <typename T>
struct IsViewType : public std::false_type {};

template <typename T>
struct TypeAsString;
#define makeAssociations(name)                                           \
    void matchInnerType(const name##Domain& domain, name##Value& value); \
    void matchInnerType(const name##Value& other, name##Value& value);   \
    void reset(name##Value& value);                                      \
    template <>                                                          \
    struct AssociatedDomain<name##Value> {                               \
        typedef name##Domain type;                                       \
    };                                                                   \
                                                                         \
    template <>                                                          \
    struct AssociatedValueType<name##Domain> {                           \
        typedef name##Value type;                                        \
    };                                                                   \
    template <>                                                          \
    struct AssociatedViewType<name##Value> {                             \
        typedef name##View type;                                         \
    };                                                                   \
    template <>                                                          \
    struct AssociatedValueType<name##View> {                             \
        typedef name##Value type;                                        \
    };                                                                   \
    template <>                                                          \
    struct TypeAsString<name##Value> {                                   \
        static const std::string value;                                  \
    };                                                                   \
    template <>                                                          \
    struct TypeAsString<name##Domain> {                                  \
        static const std::string value;                                  \
    };                                                                   \
                                                                         \
    template <>                                                          \
    struct IsDomainPtrType<std::shared_ptr<name##Domain>>                \
        : public std::true_type {};                                      \
                                                                         \
    template <>                                                          \
    struct IsDomainType<name##Domain> : public std::true_type {};        \
    template <>                                                          \
    struct IsValueType<name##Value> : public std::true_type {};          \
    template <>                                                          \
    struct IsViewType<name##View> : public std::true_type {};

buildForAllTypes(makeAssociations, );
#undef makeAssociations

template <typename T, typename U>
using EnableIfValueAndReturn =
    typename std::enable_if<IsValueType<BaseType<T>>::value, U>::type;
template <typename T>
using EnableIfValue = EnableIfValueAndReturn<T, int>;

template <typename T, typename U>
using EnableIfViewAndReturn =
    typename std::enable_if<IsViewType<BaseType<T>>::value, U>::type;
template <typename T>
using EnableIfView = EnableIfViewAndReturn<T, int>;

#endif /* SRC_BASE_TYPEDECLS_H_ */
