#ifndef SRC_OPERATORS_SETRETURNING_H_
#define SRC_OPERATORS_SETRETURNING_H_
#include <unordered_set>
#include <vector>
#include "search/violationDescription.h"
#include "types/forwardDecls/typesAndDomains.h"
#include "types/set.h"
// helper macros
#define buildForSetProducers(f, sep) \
    f(SetValue) sep f(OpSetIntersect) sep f(OpSetUnion) sep

#define structDecls(name) struct name;
buildForSetProducers(structDecls, );
#undef structDecls

using SetReturning =
    mpark::variant<ValRef<SetValue>, std::shared_ptr<OpSetIntersect>>;

#define setProducerFuncs(name)                                                 \
    void evaluate(name&);                                                      \
    void startTriggering(name&);                                               \
    void stopTriggering(name&);                                                \
    void updateViolationDescription(const name& op, u_int64_t parentViolation, \
                                    ViolationDescription&);
buildForSetProducers(setProducerFuncs, )
#undef setProducerFuncs

    inline SetView& getSetView(const SetReturning& setV) {
    return mpark::visit(
        [&](auto& setImpl) -> SetView& {
            return reinterpret_cast<SetView&>(*setImpl);
        },
        setV);
}

inline void evaluate(SetReturning& set) {
    mpark::visit([&](auto& setImpl) { evaluate(*setImpl); }, set);
}

inline void startTriggering(SetReturning& set) {
    mpark::visit([&](auto& setImpl) { startTriggering(*setImpl); }, set);
}

inline void stopTriggering(SetReturning& set) {
    mpark::visit([&](auto& setImpl) { stopTriggering(*setImpl); }, set);
}
inline void updateViolationDescription(
    const SetReturning& set, u_int64_t parentViolation,
    ViolationDescription& violationDescription) {
    mpark::visit(
        [&](auto& setImpl) {
            updateViolationDescription(*setImpl, parentViolation,
                                       violationDescription);
        },
        set);
}

#endif /* SRC_OPERATORS_SETRETURNING_H_ */
