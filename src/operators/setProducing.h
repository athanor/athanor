#ifndef SRC_OPERATORS_SETPRODUCING_H_
#define SRC_OPERATORS_SETPRODUCING_H_
#include <unordered_set>
#include <vector>
#include "search/violationDescription.h"
#include "types/forwardDecls/typesAndDomains.h"
// helper macros
#define buildForSetProducers(f, sep) \
    f(SetValue) sep f(OpSetIntersect) sep f(OpSetUnion) sep

#define structDecls(name) struct name;
buildForSetProducers(structDecls, )
#undef structDecls

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

#define setProducerFuncs(name)                                                 \
    SetView getSetView(name&);                                                 \
    void evaluate(name&);                                                      \
    void startTriggering(name&);                                               \
    void stopTriggering(name&);                                                \
    void updateViolationDescription(const name& op, u_int64_t parentViolation, \
                                    ViolationDescription&);
buildForSetProducers(setProducerFuncs, )
#undef setProducerFuncs
    inline SetView getSetView(SetProducing& set) {
    return mpark::visit([&](auto& setImpl) { return getSetView(*setImpl); },
                        set);
}

inline void evaluate(SetProducing& set) {
    mpark::visit([&](auto& setImpl) { evaluate(*setImpl); }, set);
}

inline void startTriggering(SetProducing& set) {
    mpark::visit([&](auto& setImpl) { startTriggering(*setImpl); }, set);
}

inline void stopTriggering(SetProducing& set) {
    mpark::visit([&](auto& setImpl) { stopTriggering(*setImpl); }, set);
}
inline void updateViolationDescription(
    const SetProducing& set, u_int64_t parentViolation,
    ViolationDescription& violationDescription) {
    mpark::visit(
        [&](auto& setImpl) {
            updateViolationDescription(*setImpl, parentViolation,
                                       violationDescription);
        },
        set);
}

#endif /* SRC_OPERATORS_SETPRODUCING_H_ */
