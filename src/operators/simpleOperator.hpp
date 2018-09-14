
#ifndef SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#define SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#include "base/base.h"
#include "operators/simpleOperator.h"
template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::evaluateImpl() {
    left->evaluate();
    right->evaluate();
    reevaluate();
}
template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::reevaluate() {
    auto leftView = left->getViewIfDefined();
    if (leftView) {
        auto rightView = right->getViewIfDefined();
        if (rightView) {
            this->setDefined(true);
            derived().reevaluateImpl(*leftView, *rightView);
            return;
        }
    }
    this->setDefined(false);
}

template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::startTriggeringImpl() {
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
bool SimpleBinaryOperator<View, OperandView, Derived>::optimiseImpl() {
    return false;
}

template <typename View, typename OperandView, typename Derived>
std::pair<bool, ExprRef<View>>
SimpleBinaryOperator<View, OperandView, Derived>::optimise(PathExtension path) {
    bool changeMade = false, first = true;
    while (true) {
        auto leftOptResult = left->optimise(path.extend(left));
        bool tempChangeMade = leftOptResult.first;
        left = leftOptResult.second;
        changeMade |= tempChangeMade;
        auto rightOptResult = right->optimise(path.extend(right));
        tempChangeMade |= rightOptResult.first;
        right = rightOptResult.second;

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
    return std::make_pair(changeMade, mpark::get<ExprRef<View>>(path.expr));
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::evaluateImpl() {
    operand->evaluate();
    reevaluate();
}
template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::reevaluate() {
    auto view = operand->getViewIfDefined();
    if (view) {
        this->setDefined(true);
        derived().reevaluateImpl(*view);
    }
    this->setDefined(false);
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::startTriggeringImpl() {
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
bool SimpleUnaryOperator<View, OperandView, Derived>::optimiseImpl() {
    return false;
}
template <typename View, typename OperandView, typename Derived>
std::pair<bool, ExprRef<View>>
SimpleUnaryOperator<View, OperandView, Derived>::optimise(PathExtension path) {
    bool changeMade = false, first = true;
    while (true) {
        auto optResult = operand->optimise(path.extend(operand));
        bool tempChangeMade = optResult.first;
        operand = optResult.second;
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
    return std::make_pair(changeMade, mpark::get<ExprRef<View>>(path.expr));
}
#endif /* SRC_OPERATORS_SIMPLEOPERATOR_HPP_ */
