
#ifndef SRC_OPERATORS_QUANTIFIERBASE_H_
#define SRC_OPERATORS_QUANTIFIERBASE_H_
#include "types/forwardDecls/typesAndDomains.h"

template <typename UnrollingValue>
struct UnrollTrigger : public virtual TriggerBase {
    virtual void valueChangedDuringUnroll(
        const UnrollingValue& oldValue, const ValRef<UnrollingValue>& newValue);
};

template <typename T>
struct Quantifier {
    int id;
    ValRef<T> ref;
    std::vector<std::shared_ptr<UnrollTrigger<T>>> unrollTriggers;

    Quantifier(int id, ValRef<T> ref) : id(id), ref(std::move(ref)) {}
};

template <typename T>
class QuantRef {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<Quantifier<T>> ref;

   public:
    QuantRef(int id) : ref(std::make_shared<Quantifier<T>>(id, nullptr)) {}
    inline Quantifier<T>& getQuantifier() { return *ref; }
    inline const Quantifier<T>& getQuantifier() const { return *ref; }
    inline decltype(auto) operator*() { return ref->ref.operator*(); }
    inline decltype(auto) operator-> () { return ref->ref.operator->(); }
    inline decltype(auto) operator*() const { return ref->ref.operator*(); }
    inline decltype(auto) operator-> () const { return ref->ref.operator->(); }
};

typedef Variantised<QuantRef> QuantValue;

inline static int nextQuantId() {
    static int id = 0;
    return id++;
}
#endif /* SRC_OPERATORS_QUANTIFIERBASE_H_ */
