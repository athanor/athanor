
#ifndef SRC_BASE_TYPEDECLS_H_
#define SRC_BASE_TYPEDECLS_H_
#include "../utils/variantOperations.h"
#include "base/intSize.h"
#include "base/typesMacros.h"
#include "common/common.h"
// forward declare structs
#define declDomainsAndValues(name) \
    struct name##Value;            \
    struct name##View;             \
    struct name##Domain;           \
    struct name##Trigger;
buildForAllTypes(declDomainsAndValues, )
#undef declDomainsAndValues

    // associations between values, domains and views

    template <typename T>
    struct AssociatedDomain;
template <typename T>
struct AssociatedValueType;

template <typename T>
struct AssociatedViewType;

template <typename ViewType>
struct AssociatedTriggerType;

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
    struct AssociatedTriggerType<name##View> {                           \
        typedef name##Trigger type;                                      \
    };                                                                   \
    template <>                                                          \
    struct AssociatedViewType<name##Trigger> {                           \
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

// template forward declarations.  Template specialisations are assumed to be
// implemented in CPP files.
// That is, every type must provide a specialisation of the following functions.

// short cut for building a variant of any other templated class, where the
// class is templated over a value (SetValue,IntValue, etc.)
#define variantValues(V) T<V##Value>
template <template <typename> class T>
using Variantised =
    mpark::variant<buildForAllTypes(variantValues, MACRO_COMMA)>;
#undef variantValues

template <typename Val>
EnableIfViewAndReturn<Val, bool> smallerValue(const Val& u, const Val& v);
template <typename Val>
EnableIfViewAndReturn<Val, bool> largerValue(const Val& u, const Val& v);
template <typename View>
EnableIfViewAndReturn<View, HashType> getValueHash(const View&);
template <typename View>
EnableIfViewAndReturn<View, bool> appearsDefined(const View&);
template <typename Val>
EnableIfValueAndReturn<Val, void> normalise(Val& v);
template <typename Val>
EnableIfValueAndReturn<Val, void> deepCopy(const Val& src, Val& target);
template <typename DomainType>
UInt getDomainSize(const DomainType&);
template <typename View>
EnableIfViewAndReturn<View, std::ostream&> prettyPrint(std::ostream& os,
                                                       const View&);
template <typename DomainType>
typename std::enable_if<IsDomainType<BaseType<DomainType>>::value,
                        std::ostream&>::type
prettyPrint(std::ostream& os, const DomainType&);

template <typename View>
inline EnableIfViewAndReturn<View, std::ostream&> operator<<(std::ostream& os,
                                                             const View& v) {
    return prettyPrint(os, v);
}

template <typename Domain>
inline typename std::enable_if<IsDomainType<BaseType<Domain>>::value,
                               std::ostream&>::type
operator<<(std::ostream& os, const Domain& d) {
    return prettyPrint(os, d);
}

template <typename Val, typename View = typename AssociatedViewType<Val>::type,
          EnableIfValue<Val> = 0>
View& asView(Val& val) {
    return reinterpret_cast<View&>(val);
}

template <typename Val, typename View = typename AssociatedViewType<Val>::type,
          EnableIfValue<Val> = 0>
const View& asView(const Val& val) {
    return reinterpret_cast<const View&>(val);
}
#endif /* SRC_BASE_TYPEDECLS_H_ */
