
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
    void evaluate() final {
        left->evaluate();
        right->evaluate();
        derived().reevaluate();
    }

    void startTriggering() final {
        if (!leftTrigger) {
            leftTrigger = std::make_shared<LeftTrigger>(&derived());
            rightTrigger = std::make_shared<RightTrigger>(&derived());
            left->addTrigger(leftTrigger);
            right->addTrigger(rightTrigger);
            left->startTriggering();
            right->startTriggering();
        }
    }

    void stopTriggeringOnChildren() {
        if (leftTrigger) {
            deleteTrigger(leftTrigger);
            leftTrigger = nullptr;
            deleteTrigger(rightTrigger);
            rightTrigger = nullptr;
        }
    }

    void stopTriggering() final {
        if (leftTrigger) {
            stopTriggeringOnChildren();
            left->stopTriggering();
            right->stopTriggering();
        }
    }

    ExprRef<View> deepCopySelfForUnroll(
        const ExprRef<View>&, const AnyIterRef& iterator) const final {
        auto newOp = std::make_shared<Derived>(
            left->deepCopySelfForUnroll(left, iterator),
            right->deepCopySelfForUnroll(right, iterator));
        derived().copy(*newOp);
        return newOp;
    }

    void findAndReplaceSelf(const FindAndReplaceFunction& func) final {
        this->left = findAndReplace(left, func);
        this->right = findAndReplace(right, func);
    }
};
#endif /* SRC_OPERATORS_SIMPLEOPERATOR_H_ */
