
#ifndef SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#define SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#include "base/base.h"
#include "operators/simpleOperator.h"
template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::evaluateImpl() {
    left->evaluate();
    right->evaluate();
    bool defined = !(left->isUndefined() || right->isUndefined());
    this->setDefined(defined);
    if (defined) {
        derived().reevaluate();
    }
}

template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::startTriggering() {
    if (!leftTrigger) {
        leftTrigger = std::make_shared<LeftTrigger>(&derived());
        rightTrigger = std::make_shared<RightTrigger>(&derived());
        left->addTrigger(leftTrigger);
        right->addTrigger(rightTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView,
                          Derived>::stopTriggeringOnChildren() {
    if (leftTrigger) {
        deleteTrigger(leftTrigger);
        leftTrigger = nullptr;
        deleteTrigger(rightTrigger);
        rightTrigger = nullptr;
    }
}

template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::stopTriggering() {
    if (leftTrigger) {
        stopTriggeringOnChildren();
        left->stopTriggering();
        right->stopTriggering();
    }
}

template <typename View, typename OperandView, typename Derived>
ExprRef<View>
SimpleBinaryOperator<View, OperandView, Derived>::deepCopySelfForUnrollImpl(
    const ExprRef<View>&, const AnyIterRef& iterator) const {
    auto newOp = std::make_shared<Derived>(
        left->deepCopySelfForUnroll(left, iterator),
        right->deepCopySelfForUnroll(right, iterator));
    this->copyDefinedStatus(*newOp);
    derived().copy(*newOp);
    return newOp;
}

template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->left = findAndReplace(left, func);
    this->right = findAndReplace(right, func);
}

template <typename View, typename OperandView, typename Derived>
bool SimpleBinaryOperator<View, OperandView, Derived>::isUndefined() {
    return !this->isDefined();
}
template <typename View, typename OperandView, typename Derived>
bool SimpleBinaryOperator<View, OperandView, Derived>::optimiseImpl() {
    return false;
}

template <typename View, typename OperandView, typename Derived>
bool SimpleBinaryOperator<View, OperandView, Derived>::optimise(
    PathExtension path) {
    bool changeMade = false, first = true;
    while (true) {
        bool tempChangeMade = left->optimise(path.extend(left));
        changeMade |= tempChangeMade;
        tempChangeMade |= right->optimise(path.extend(right));
        changeMade |= tempChangeMade;
        if (first || tempChangeMade) {
            first = false;
            tempChangeMade = derived().optimiseImpl();
            changeMade |= tempChangeMade;
            if (tempChangeMade) {
                continue;
            }
        }
        break;
    }
    return changeMade;
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::evaluateImpl() {
    operand->evaluate();
    bool defined = !operand->isUndefined();
    this->setDefined(defined);
    if (defined) {
        derived().reevaluate();
    }
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::startTriggering() {
    if (!operandTrigger) {
        operandTrigger = std::make_shared<OperandTrigger>(&derived());
        operand->addTrigger(operandTrigger);
        operand->startTriggering();
    }
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView,
                         Derived>::stopTriggeringOnChildren() {
    if (operandTrigger) {
        deleteTrigger(operandTrigger);
        operandTrigger = nullptr;
    }
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::stopTriggering() {
    if (operandTrigger) {
        stopTriggeringOnChildren();
        operand->stopTriggering();
    }
}

template <typename View, typename OperandView, typename Derived>
ExprRef<View>
SimpleUnaryOperator<View, OperandView, Derived>::deepCopySelfForUnrollImpl(
    const ExprRef<View>&, const AnyIterRef& iterator) const {
    auto newOp = std::make_shared<Derived>(
        operand->deepCopySelfForUnroll(operand, iterator));
    this->copyDefinedStatus(*newOp);
    derived().copy(*newOp);
    return newOp;
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->operand = findAndReplace(operand, func);
}

template <typename View, typename OperandView, typename Derived>
bool SimpleUnaryOperator<View, OperandView, Derived>::isUndefined() {
    return !this->isDefined();
}
template <typename View, typename OperandView, typename Derived>
bool SimpleUnaryOperator<View, OperandView, Derived>::optimiseImpl() {
    return false;
}
template <typename View, typename OperandView, typename Derived>
bool SimpleUnaryOperator<View, OperandView, Derived>::optimise(
    PathExtension path) {
    bool changeMade = false, first = true;
    while (true) {
        bool tempChangeMade = operand->optimise(path.extend(operand));
        changeMade |= tempChangeMade;
        if (first || tempChangeMade) {
            first = false;
            tempChangeMade = derived().optimiseImpl();
            changeMade |= tempChangeMade;
            if (tempChangeMade) {
                continue;
            }
        }
        break;
    }
    return changeMade;
}
#endif /* SRC_OPERATORS_SIMPLEOPERATOR_HPP_ */
