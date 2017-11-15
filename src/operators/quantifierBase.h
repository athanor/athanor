
#ifndef SRC_OPERATORS_QUANTIFIERBASE_H_
#define SRC_OPERATORS_QUANTIFIERBASE_H_
#include <cassert>
#include <unordered_map>
#include <vector>
#include "types/forwardDecls/typesAndDomains.h"

#include "types/forwardDecls/hash.h"

template <typename UnrollingValue>
struct UnrollTrigger : public virtual TriggerBase {
    virtual void valueChangedDuringUnroll(
        const UnrollingValue& oldValue, const ValRef<UnrollingValue>& newValue);
};

template <typename T>
struct Iterator {
    int id;
    ValRef<T> ref;
    std::vector<std::shared_ptr<UnrollTrigger<T>>> unrollTriggers;

    Iterator(int id, ValRef<T> ref) : id(id), ref(std::move(ref)) {}
    inline void attachValue(const ValRef<T>& val) { ref = val; }
};

template <typename T>
class IterRef {
   public:
    typedef T element_type;

   private:
    std::shared_ptr<Iterator<T>> ref;

   public:
    IterRef(int id)
        : ref(std::make_shared<Iterator<T>>(id, ValRef<T>(nullptr))) {}
    inline Iterator<T>& getIterator() { return *ref; }
    inline decltype(auto) refCount() { return ref.use_count(); }
    inline const Iterator<T>& getIterator() const { return *ref; }
    inline decltype(auto) operator*() { return ref->ref.operator*(); }
    inline decltype(auto) operator-> () { return ref->ref.operator->(); }
    inline decltype(auto) operator*() const { return ref->ref.operator*(); }
    inline decltype(auto) operator-> () const { return ref->ref.operator->(); }
};

typedef Variantised<IterRef> IterValue;

inline static int nextQuantId() {
    static int id = 0;
    return id++;
}

template <typename ContainerType, typename ReturnType, typename ReturnTypeValue,
          typename ReturnTypeView>
struct Quantifier : public ReturnTypeView {
    const int quantId = nextQuantId();
    ContainerType set;
    ReturnType expr = ValRef<ReturnTypeValue>(nullptr);
    std::vector<std::pair<ReturnType, IterValue>> unrolledExprs;
    std::unordered_map<u_int64_t, size_t> valueExprMap;

    Quantifier(ContainerType set) : set(std::move(set)) {}

    inline void setExpression(ReturnType exprIn) { expr = std::move(exprIn); }

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

    inline void unroll(const Value& newValue) {
        mpark::visit(
            [&](auto& newValImpl) {
                auto quantRef = newIterRef<
                    typename BaseType<decltype(newValImpl)>::element_type>();
                unrolledExprs.emplace_back(deepCopyForUnroll(expr, quantRef),
                                           quantRef);
                valueExprMap.emplace(getValueHash(newValImpl),
                                     unrolledExprs.size() - 1);
                quantRef.getIterator().attachValue(newValImpl);
            },
            newValue);
    }

    inline void roll(const Value& val) {
        u_int64_t hash = getValueHash(val);
        assert(valueExprMap.count(hash));
        size_t index = valueExprMap[hash];
        unrolledExprs[index] = std::move(unrolledExprs.back());
        unrolledExprs.pop_back();
        valueExprMap.erase(hash);
    }
};

#endif /* SRC_OPERATORS_QUANTIFIERBASE_H_ */
