
#ifndef SRC_OPERATORS_OPCATCHUNDEF_H_
#define SRC_OPERATORS_OPCATCHUNDEF_H_

#include "base/base.h"
#include "triggers/allTriggers.h"

template <typename ExprViewType>
struct OpCatchUndef : public ExprInterface<ExprViewType>,
                      public TriggerContainer<ExprViewType> {
    typedef typename AssociatedTriggerType<ExprViewType>::type ExprTriggerType;
    struct ExprTrigger;
    ExprRef<ExprViewType> expr;
    ExprRef<ExprViewType> replacement;
    bool exprDefined = false;
    bool recentlyTriggeredChange = false;
    std::shared_ptr<ExprTrigger> exprTrigger;

    OpCatchUndef(ExprRef<ExprViewType> expr, ExprRef<ExprViewType> replacement)
        : expr(std::move(expr)), replacement(std::move(replacement)) {}
    OpCatchUndef(const OpCatchUndef<ExprViewType>&) = delete;
    OpCatchUndef(OpCatchUndef<ExprViewType>&&) = delete;
    ~OpCatchUndef() { this->stopTriggeringOnChildren(); }
    bool allowForwardingOfTrigger();
    void setAppearsDefined(bool set);
    void addTriggerImpl(const std::shared_ptr<ExprTriggerType>& trigger,
                        bool includeMembers, Int memberIndex) final;
    OptionalRef<ExprViewType> view() final;
    OptionalRef<const ExprViewType> view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<ExprViewType> deepCopyForUnrollImpl(
        const ExprRef<ExprViewType>& self,
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    ExprRef<ExprViewType>& getMember();
    const ExprRef<ExprViewType>& getMember() const;
    void reevaluate();
    std::pair<bool, ExprRef<ExprViewType>> optimiseImpl(
        ExprRef<ExprViewType>&, PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

#endif /* SRC_OPERATORS_OPCATCHUNDEF_H_ */
