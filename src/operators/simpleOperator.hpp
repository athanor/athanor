
#ifndef SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#define SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#include "base/base.h"
#include "operators/simpleOperator.h"
template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::evaluateImpl() {
    left->evaluate();
    right->evaluate();
    reevaluate(true, true);
}
template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView, Derived>::reevaluate(
    bool leftChanged, bool rightChanged) {
    auto leftView = left->getViewIfDefined();
    if (leftView) {
        auto rightView = right->getViewIfDefined();
        if (rightView) {
            this->setDefined(true);
            derived().reevaluateImpl(*leftView, *rightView, leftChanged,
                                     rightChanged);
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
SimpleBinaryOperator<View, OperandView, Derived>::deepCopyForUnrollImpl(
    const ExprRef<View>&, const AnyIterRef& iterator) const {
    auto newOp =
        std::make_shared<Derived>(left->deepCopyForUnroll(left, iterator),
                                  right->deepCopyForUnroll(right, iterator));
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
bool SimpleBinaryOperator<View, OperandView, Derived>::optimiseImpl(
    const PathExtension&) {
    return false;
}

template <typename View, typename OperandView, typename Derived>
std::pair<bool, ExprRef<View>>
SimpleBinaryOperator<View, OperandView, Derived>::optimise(PathExtension path) {
    bool changeMade = false;
    auto leftOptResult = left->optimise(path.extend(left));
    changeMade |= leftOptResult.first;
    left = leftOptResult.second;
    auto rightOptResult = right->optimise(path.extend(right));
    changeMade |= rightOptResult.first;
    right = rightOptResult.second;
    changeMade |= derived().optimiseImpl(path);
    return std::make_pair(changeMade, mpark::get<ExprRef<View>>(path.expr));
}

template <typename View, typename OperandView, typename Derived>
void SimpleBinaryOperator<View, OperandView,
                          Derived>::standardSanityDefinednessChecks() const {
    if (!left->getViewIfDefined() || !right->getViewIfDefined()) {
        if (this->appearsDefined()) {
            throw SanityCheckException(
                "Operands are undefined but operator is still defined.");
        }
    }
    if (!this->appearsDefined()) {
        throw SanityCheckException("operands are defined but operator is not.");
    }
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::evaluateImpl() {
    operand->evaluate();
    reevaluate();
}
template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView, Derived>::reevaluate(
    bool, bool) {  // ignore bools, they are there to make it easier to compile
                   // between unary and binary ops
    auto view = operand->getViewIfDefined();
    if (view) {
        this->setDefined(true);
        derived().reevaluateImpl(*view);
    } else {
        this->setDefined(false);
    }
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
SimpleUnaryOperator<View, OperandView, Derived>::deepCopyForUnrollImpl(
    const ExprRef<View>&, const AnyIterRef& iterator) const {
    auto newOp = std::make_shared<Derived>(
        operand->deepCopyForUnroll(operand, iterator));
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
bool SimpleUnaryOperator<View, OperandView, Derived>::optimiseImpl(
    const PathExtension&) {
    return false;
}
template <typename View, typename OperandView, typename Derived>
std::pair<bool, ExprRef<View>>
SimpleUnaryOperator<View, OperandView, Derived>::optimise(PathExtension path) {
    bool changeMade = false;
    auto optResult = operand->optimise(path.extend(operand));
    changeMade |= optResult.first;
    operand = optResult.second;
    changeMade |= derived().optimiseImpl(path);
    return std::make_pair(changeMade, mpark::get<ExprRef<View>>(path.expr));
}
template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView,
                         Derived>::standardSanityDefinednessChecks() const {
    if (!operand->getViewIfDefined()) {
        if (this->appearsDefined()) {
            throw SanityCheckException(
                "Operand is undefined but operator is still defined.");
        }
    }
    if (!this->appearsDefined()) {
        throw SanityCheckException("operand is defined but operator is not.");
    }
}

#endif /* SRC_OPERATORS_SIMPLEOPERATOR_HPP_ */
