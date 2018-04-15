
#ifndef SRC_OPERATORS_SIMPLEOPERATOR_H_
#define SRC_OPERATORS_SIMPLEOPERATOR_H_
#include "base/base.h"

template <typename Derived>
struct OperatorTrates;
template <typename View, typename OperandView, typename Derived>
struct SimpleBinaryOperator : public View {
    typedef typename OperatorTrates<Derived>::LeftTrigger LeftTrigger;
    typedef typename OperatorTrates<Derived>::RightTrigger RightTrigger;
    ExprRef<OperandView> left;
    ExprRef<IntView> right;
    std::shared_ptr<LeftTrigger> leftTrigger;
    std::shared_ptr<RightTrigger> rightTrigger;

   public:
    SimpleBinaryOperator(ExprRef<OperandView> left, ExprRef<OperandView> right)
        : left(std::move(left)), right(std::move(right)) {}

    auto& derived() { return *static_cast<Derived*>(this); }
    const auto& derived() const { return *static_cast<const Derived*>(this); }
    SimpleBinaryOperator(
        const SimpleBinaryOperator<View, OperandView, Derived>& other) = delete;
    SimpleBinaryOperator(
        SimpleBinaryOperator<View, OperandView, Derived>&& other)
        : left(std::move(other.left)),
          right(std::move(other.right)),
          leftTrigger(std::move(other.leftTrigger)),
          rightTrigger(std::move(other.rightTrigger)) {
        setTriggerParent(&derived(), leftTrigger, rightTrigger);
    }
    virtual ~SimpleBinaryOperator() { this->stopTriggeringOnChildren(); }

    void evaluate() final;
    void startTriggering() final;
    void stopTriggeringOnChildren();
    void stopTriggering() final;
    ExprRef<View> deepCopySelfForUnroll(const ExprRef<View>&,
                                        const AnyIterRef& iterator) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction& func) final;
};

template <typename View, typename OperandView, typename Derived>
struct SimpleUnaryOperator : public View {
    typedef typename OperatorTrates<Derived>::OperandTrigger OperandTrigger;
    ExprRef<OperandView> operand;
    std::shared_ptr<OperandTrigger> operandTrigger;

    SimpleUnaryOperator(ExprRef<OperandView> operand)
        : operand(std::move(operand)) {}

    auto& derived() { return *static_cast<Derived*>(this); }
    const auto& derived() const { return *static_cast<const Derived*>(this); }
    SimpleUnaryOperator(
        const SimpleUnaryOperator<View, OperandView, Derived>& other) = delete;
    SimpleUnaryOperator(SimpleUnaryOperator<View, OperandView, Derived>&& other)
        : operand(std::move(other.operand)),
          operandTrigger(std::move(other.operandTrigger)) {
        setTriggerParent(&derived(), operandTrigger);
    }
    virtual ~SimpleUnaryOperator() { this->stopTriggeringOnChildren(); }
    void evaluate() final;
    void startTriggering() final;
    void stopTriggeringOnChildren();
    void stopTriggering() final;
    ExprRef<View> deepCopySelfForUnroll(const ExprRef<View>&,
                                        const AnyIterRef& iterator) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction& func) final;
};

#endif /* SRC_OPERATORS_SIMPLEOPERATOR_H_ */
