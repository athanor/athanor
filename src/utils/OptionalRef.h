
#ifndef SRC_UTILS_OPTIONALREF_H_
#define SRC_UTILS_OPTIONALREF_H_

/**
 * Class to hold an optional reference.  Class stores a pointer to an object, it
 * does not have ownership.  It can be queried if it has a value and the value
 * can be retrieved as a reference, equivalent to dereferencing the internal
 * pointer.
 */
#include <cassert>
#include "common/common.h"

class EmptyOptional {};
template <typename T>
class OptionalRef {
    T* ptr;

   public:
    OptionalRef(T& val) : ptr(&val) {}
    OptionalRef(T&&) = delete;
    OptionalRef(EmptyOptional) : ptr(NULL) {}
    OptionalRef() : ptr(NULL) {}
    inline bool hasValue() const { return ptr != NULL; }
    inline operator bool() const { return hasValue(); }
    inline T& get() const {
        debug_code(assert(hasValue()));

        return *ptr;
    }
    inline T& operator*() const { return get(); }
};

template <typename T>
inline OptionalRef<T> makeOptional(T& val) {
    return OptionalRef<T>(val);
}
#endif /* SRC_UTILS_OPTIONALREF_H_ */
