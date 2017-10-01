#ifndef SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_
#define SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_
#include <utils/stackFunction.h>
#include <functional>
#include <iostream>
#include <memory>
#include "utils/variantOperations.h"
#define buildForAllTypes(f, sep) f(Bool) sep f(Int) sep f(Set)

#define MACRO_COMMA ,

template <typename T>
using BaseType =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;
// forward declare structs
#define declDomainsAndValues(name) \
    struct name##Value;            \
    struct name##Domain;
buildForAllTypes(declDomainsAndValues, )
#undef declDomainsAndValues

// declare variant for values
#define variantValues(T) std::shared_ptr<T##Value>
    using Value = mpark::variant<buildForAllTypes(variantValues, MACRO_COMMA)>;
#undef variantValues

// declare variant for domains
#define variantDomains(T) std::shared_ptr<T##Domain>
using Domain = mpark::variant<buildForAllTypes(variantDomains, MACRO_COMMA)>;
#undef variantDomains
// calll back used by nested values to determine if their parent allows such a
// value
typedef StackFunction<bool()> ParentCheck;
// mapping from values to domains and back

template <typename T>
struct AssociatedDomain;
template <typename T>
struct AssociatedValueType;

template <typename T>
struct IsDomainPtrType : public std::false_type {};
template <typename T>
struct IsDomainType : public std::false_type {};

template <typename T>
struct TypeAsString;
#define makeAssociations(name)                            \
    template <>                                           \
    struct AssociatedDomain<name##Value> {                \
        typedef name##Domain type;                        \
    };                                                    \
                                                          \
    template <>                                           \
    struct AssociatedValueType<name##Domain> {            \
        typedef name##Value type;                         \
    };                                                    \
    template <>                                           \
    struct TypeAsString<name##Value> {                    \
        static const std::string value;                   \
    };                                                    \
    template <>                                           \
    struct TypeAsString<name##Domain> {                   \
        static const std::string value;                   \
    };                                                    \
                                                          \
    template <>                                           \
    struct IsDomainPtrType<std::shared_ptr<name##Domain>> \
        : public std::true_type {};                       \
                                                          \
    template <>                                           \
    struct IsDomainType<name##Domain> : public std::true_type {};

buildForAllTypes(makeAssociations, )
#undef makeAssociations

    template <typename DomainType>
    inline std::shared_ptr<typename AssociatedValueType<
        DomainType>::type>& getAssociatedValue(DomainType&, Value& value) {
    return mpark::get<
        std::shared_ptr<typename AssociatedValueType<DomainType>::type>>(value);
}
template <typename DomainType>
inline const std::shared_ptr<typename AssociatedValueType<DomainType>::type>&
getAssociatedValue(DomainType&, const Value& value) {
    return mpark::get<
        std::shared_ptr<typename AssociatedValueType<DomainType>::type>>(value);
}
enum class ValueState { CLEAN, DIRTY, UNDEFINED };
struct DirtyFlag {
    ValueState state = ValueState::UNDEFINED;
    DirtyFlag() {}
    DirtyFlag(ValueState state) : state(state) {}
};
#define dirtyStateFunctions(name)                    \
    ValueState dirtyState(const name##Value& value); \
                                                     \
    ValueState& dirtyState(name##Value& value);
buildForAllTypes(dirtyStateFunctions, )
#undef dirtyStateFunctions

    // size attr for domains
    struct SizeAttr {
    enum class SizeAttrType {
        NO_SIZE,
        EXACT_SIZE,
        MIN_SIZE,
        MAX_SIZE,
        SIZE_RANGE
    };
    SizeAttrType sizeType;
    size_t minSize, maxSize;

   private:
    SizeAttr(SizeAttrType type, size_t minSize, size_t maxSize)
        : sizeType(type), minSize(minSize), maxSize(maxSize) {}

   public:
    friend inline SizeAttr noSize() {
        return SizeAttr(SizeAttrType::NO_SIZE, 0,
                        std::numeric_limits<uint64_t>::max());
    }
    friend inline SizeAttr exactSize(size_t size) {
        return SizeAttr(SizeAttrType::EXACT_SIZE, size, size);
    }
    friend inline SizeAttr minSize(size_t minSize) {
        return SizeAttr(SizeAttrType::MIN_SIZE, minSize,
                        std::numeric_limits<uint64_t>::max());
    }
    friend inline SizeAttr maxSize(size_t maxSize) {
        return SizeAttr(SizeAttrType::MAX_SIZE, 0, maxSize);
    }

    friend inline SizeAttr sizeRange(size_t minSize, size_t maxSize) {
        return SizeAttr(SizeAttrType::SIZE_RANGE, minSize, maxSize);
    }
    friend inline std::ostream& operator<<(std::ostream& os,
                                           const SizeAttr& s) {
        switch (s.sizeType) {
            case SizeAttrType::NO_SIZE:
                os << "noSize";
                break;
            case SizeAttrType::MIN_SIZE:
                os << "minSize " << s.minSize;
                break;
            case SizeAttrType::MAX_SIZE:
                os << "maxSize " << s.maxSize;
                break;
            case SizeAttrType::EXACT_SIZE:
                os << "size " << s.minSize;
                break;
            case SizeAttrType::SIZE_RANGE:
                os << "minSize " << s.minSize << ", maxSize " << s.maxSize;
        }
        return os;
    }
};

SizeAttr noSize();
inline SizeAttr exactSize(size_t size);
SizeAttr minSize(size_t minSize);
SizeAttr maxSize(size_t maxSize);
SizeAttr sizeRange(size_t minSize, size_t maxSize);

#endif /* SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_ */
