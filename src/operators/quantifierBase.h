
#ifndef SRC_OPERATORS_QUANTIFIERBASE_H_
#define SRC_OPERATORS_QUANTIFIERBASE_H_
#include "types/forwardDecls/typesAndDomains.h"

template <typename T>
class QuantRef {
   public:
    typedef T element_type;
    int id;

   private:
    std::shared_ptr<ValRef<T>> ref;

   public:
    QuantRef(const int id)
        : id(id), ref(std::make_shared<ValRef<T>>(nullptr)) {}

    inline void attachValue(const ValRef<T>& ref) { *ref = ref; }
    inline void attachValue(const Value& val) {
        attachValue(mpark::get<ValRef<T>>(val));
    }
    inline void releaseValue() { *ref = ValRef<T>(nullptr); }
    inline decltype(auto) operator*() { return ref->operator*(); }
    inline decltype(auto) operator-> () { return ref->operator->(); }
    inline decltype(auto) operator*() const { return ref->operator*(); }
    inline decltype(auto) operator-> () const { return ref->operator->(); }
};

typedef Variantised<QuantRef> QuantValue;

inline static int nextQuantId() {
    static int id = 0;
    return id++;
}
#endif /* SRC_OPERATORS_QUANTIFIERBASE_H_ */
