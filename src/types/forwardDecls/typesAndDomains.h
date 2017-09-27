#ifndef SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_
#define SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_
#include <functional>
#include <memory>
#include "utils/variantOperations.h"
#define buildForAllTypes(f, sep) f(Int) sep f(Set)

#define MACRO_COMMA ,

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

// mapping from values to domains and back

template <typename T>
struct AssociatedDomain;
template <typename T>
struct AssociatedValueType;

#define makeAssociations(name)                 \
    template <>                                \
    struct AssociatedDomain<name##Value> {     \
        typedef name##Domain type;             \
    };                                         \
                                               \
    template <>                                \
    struct AssociatedValueType<name##Domain> { \
        typedef name##Value type;              \
    };

buildForAllTypes(makeAssociations, )
#undef makeAssociations

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
};

#endif /* SRC_TYPES_FORWARDDECLS_TYPESANDDOMAINS_H_ */
