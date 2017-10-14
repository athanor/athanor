#ifndef SRC_OPERATORS_SETPRODUCING_H_
#define SRC_OPERATORS_SETPRODUCING_H_
#include <unordered_set>
#include <vector>
#include "types/forwardDecls/typesAndDomains.h"

struct OpSetIntersect;
struct OpSetUnion;
struct SetTrigger {
    virtual void valueRemoved(const Value& member) = 0;
    virtual void valueAdded(const Value& member) = 0;
    virtual void possibleValueChange(const Value& member) = 0;
    virtual void valueChanged(const Value& member) = 0;
};

using SetProducing =
    mpark::variant<ValRef<SetValue>, std::shared_ptr<OpSetIntersect>>;

struct SetView {
    std::unordered_set<u_int64_t>& memberHashes;
    u_int64_t& cachedHashTotal;
    std::vector<std::shared_ptr<SetTrigger>>& triggers;
    SetView(std::unordered_set<u_int64_t>& memberHashes,
            u_int64_t& cachedHashTotal,
            std::vector<std::shared_ptr<SetTrigger>>& triggers)
        : memberHashes(memberHashes),
          cachedHashTotal(cachedHashTotal),
          triggers(triggers) {}
};

SetView getSetView(SetValue& value);
SetView getSetView(OpSetIntersect& op);
SetView getSetView(OpSetUnion& op);

inline SetView getSetView(SetProducing& set) {
    return mpark::visit([&](auto& setImpl) { return getSetView(*setImpl); },
                        set);
}
#endif /* SRC_OPERATORS_SETPRODUCING_H_ */
