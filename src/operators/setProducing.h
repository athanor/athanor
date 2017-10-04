#ifndef SRC_OPERATORS_SETPRODUCING_H_
#define SRC_OPERATORS_SETPRODUCING_H_
#include <unordered_set>
#include "types/forwardDecls/typesAndDomains.h"

struct OpSetIntersect;
struct OpSetUnion;
struct SetTrigger {
    virtual void valueRemoved(const SetValue& container,
                              const Value& member) = 0;
    virtual void valueAdded(const SetValue& container, const Value& member) = 0;
    virtual void preValueChange(const SetValue& container,
                                const Value& member) = 0;
    virtual void postValueChange(const SetValue& container,
                                 const Value& member) = 0;
};

using SetProducing =
    mpark::variant<std::shared_ptr<SetValue>, std::shared_ptr<OpSetIntersect>,
                   std::shared_ptr<OpSetUnion>>;

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

#endif /* SRC_OPERATORS_SETPRODUCING_H_ */
