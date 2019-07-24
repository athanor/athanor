
#ifndef SRC_OPERATORS_OPUNDEFINED_H_
#define SRC_OPERATORS_OPUNDEFINED_H_
#include "base/base.h"
#include "triggers/allTriggers.h"
#include "types/allVals.h"
template <typename View>
struct OpUndefined : public ExprInterface<View>, public TriggerContainer<View> {
    typedef typename AssociatedTriggerType<View>::type Trigger;
    OpUndefined() { this->setConstant(true); }
    OpUndefined(const OpUndefined<View>&) = delete;
    OpUndefined(OpUndefined<View>&& other) = delete;
    ~OpUndefined() {}
    void addTriggerImpl(const std::shared_ptr<Trigger>& trigger,
                        bool includeMembers, Int memberIndex) final;
    OptionalRef<View> view() final;
    OptionalRef<const View> view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<View> deepCopyForUnrollImpl(const ExprRef<View>& self,
                                        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;

    std::pair<bool, ExprRef<View>> optimise(PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPUNDEFINED_H_ */
