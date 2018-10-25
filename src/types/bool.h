#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>
#include "triggers/boolTrigger.h"
#include "utils/ignoreUnused.h"

#include "base/base.h"
struct BoolDomain {};
struct BoolView : public ExprInterface<BoolView> {
    UInt violation;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;

    inline void initFrom(BoolView& other) { violation = other.violation; }
    template <typename Func>
    inline bool changeValue(Func&& func) {
        UInt oldViolation = violation;
        if (func() && violation != oldViolation) {
            visitTriggers([&](auto& trigger) { trigger->valueChanged(); },
                          triggers);
            return true;
        }
        return false;
    }
};

struct BoolValue : public BoolView, ValBase {
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<BoolView> deepCopyForUnrollImpl(
        const ExprRef<BoolView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;

    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<BoolView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_BOOL_H_ */
