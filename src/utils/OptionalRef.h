
#ifndef SRC_UTILS_OPTIONALREF_H_
#define SRC_UTILS_OPTIONALREF_H_
#include <string>

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
    //    friend class OptionalRef<typename std::remove_cv<T>::type>;
    friend class OptionalRef<const T>;
    T* ptr;

   public:
    OptionalRef(T& val) : ptr(&val) {}
    // allow casting from non const to const
    OptionalRef(const OptionalRef<typename std::remove_cv<T>::type>& other)
        : ptr(other.ptr) {}
    OptionalRef(T&&) = delete;
    OptionalRef(EmptyOptional) : ptr(NULL) {}
    OptionalRef() : ptr(NULL) {}
    inline bool hasValue() const { return ptr != NULL; }
    inline operator bool() const { return hasValue(); }
    inline T& get() {
        debug_code(assert(hasValue()));
        return *ptr;
    }
    inline T* operator->() {
        debug_code(assert(hasValue()));
        return ptr;
    }

    inline T& get() const {
        debug_code(assert(hasValue()));
        return *ptr;
    }
    inline T* operator->() const {
        debug_code(assert(hasValue()));
        return ptr;
    }
    inline T& checkedGet(const std::string& errorMessage) {
        if (!hasValue()) {
            myCerr << errorMessage << std::endl;
            myAbort();
        }
        return *ptr;
    }
    inline T& checkedGet(const std::string& errorMessage) const {
        if (!hasValue()) {
            myCerr << errorMessage << std::endl;
            myAbort();
        }
        return *ptr;
    }
    inline T& operator*() const { return get(); }
};

template <typename T>
inline OptionalRef<T> makeOptional(T& val) {
    return OptionalRef<T>(val);
}
#endif /* SRC_UTILS_OPTIONALREF_H_ */
