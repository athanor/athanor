#ifndef SRC_OPERATORS_QUANTIFIER_H_
#include "operators/quantifierView.h"
#define SRC_OPERATORS_QUANTIFIER_H_
#include "base/base.h"
#include "types/allTypes.h"

template <typename ContainerType>
struct Quantifier : public SequenceView {
    const int quantId;
    ExprRef<ContainerType> container;
    AnyExprRef expr;
    std::vector<AnyIterRef> unrolledIterVals;
    std::shared_ptr<ContainerTrigger<ContainerType, ExprType>> containerTrigger;

    Quantifier(ExprRef<ContainerType> container, const int id = nextQuantId());
    Quantifier(const Quantifier<ContainerType, ExprRef<ExprType>>&& other);
    ~Quantifier() { this->stopTriggering(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<BoolView> deepCopySelfForUnroll(
        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;

    template <typename T>
    inline IterRef<T> newIterRef() {
        return IterRef<T>(quantId);
    }

   private:
    bool triggering();
    template <typename ViewType>
    void unroll(const ExprRef<ViewType>& newView);
    void roll(UInt index);
};

#endif /* SRC_OPERATORS_QUANTIFIER_H_ */
