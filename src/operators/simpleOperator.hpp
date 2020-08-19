
#ifndef SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#define SRC_OPERATORS_SIMPLEOPERATOR_HPP_
#include "base/base.h"
#include "operators/simpleOperator.h"
template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
void SimpleBinaryOperator<View, LeftOperandView, RightOperandView,
                          Derived>::evaluateImpl() {
    left->evaluate();
    right->evaluate();
    reevaluate(true, true);
}
template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
void SimpleBinaryOperator<View, LeftOperandView, RightOperandView,
                          Derived>::reevaluate(bool leftChanged,
                                               bool rightChanged) {
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

template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
void SimpleBinaryOperator<View, LeftOperandView, RightOperandView,
                          Derived>::startTriggeringImpl() {
    if (!leftTrigger) {
        leftTrigger = std::make_shared<LeftTrigger>(&derived());
        rightTrigger = std::make_shared<RightTrigger>(&derived());
        left->addTrigger(leftTrigger);
        right->addTrigger(rightTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
void SimpleBinaryOperator<View, LeftOperandView, RightOperandView,
                          Derived>::stopTriggering() {
    if (leftTrigger) {
        left->stopTriggering();
        right->stopTriggering();
    }
}

template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
ExprRef<View> SimpleBinaryOperator<
    View, LeftOperandView, RightOperandView,
    Derived>::deepCopyForUnrollImpl(const ExprRef<View>&,
                                    const AnyIterRef& iterator) const {
    auto newOp =
        std::make_shared<Derived>(left->deepCopyForUnroll(left, iterator),
                                  right->deepCopyForUnroll(right, iterator));
    this->copyDefinedStatus(*newOp);
    derived().copy(*newOp);
    return newOp;
}

template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
void SimpleBinaryOperator<View, LeftOperandView, RightOperandView, Derived>::
    findAndReplaceSelf(const FindAndReplaceFunction& func, PathExtension path) {
    this->left = findAndReplace(left, func, path);
    this->right = findAndReplace(right, func, path);
}

template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
std::pair<bool, std::shared_ptr<Derived>>
SimpleBinaryOperator<View, LeftOperandView, RightOperandView,
                     Derived>::standardOptimise(ExprRef<View>&,
                                                PathExtension& path) {
    auto newOp = std::make_shared<Derived>(left, right);
    derived().copy(*newOp);
    bool optimised = false;
    optimised |= optimise(ExprRef<View>(newOp), newOp->left, path);
    optimised |= optimise(ExprRef<View>(newOp), newOp->right, path);
    return std::make_pair(optimised, newOp);
}

// this assumes that Derived can be constructed with operands left and right, if
// we ever get to a point that one of them cannot, use the arrow return type
// trick to  disable this function. i.e. use auto ...::optimise(...)  ->
// decltype(std::make_shared<Derived>) instead of
template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
std::pair<bool, ExprRef<View>>
SimpleBinaryOperator<View, LeftOperandView, RightOperandView,
                     Derived>::optimiseImpl(ExprRef<View>& self,
                                            PathExtension path) {
    return standardOptimise(self, path);
}

template <typename View, typename LeftOperandView, typename RightOperandView,
          typename Derived>
void SimpleBinaryOperator<View, LeftOperandView, RightOperandView,
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
    bool, bool) {  // ignore bools, they are there to make it easier
                   // to compile between unary and binary ops
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
void SimpleUnaryOperator<View, OperandView, Derived>::stopTriggering() {
    if (operandTrigger) {
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
    const FindAndReplaceFunction& func, PathExtension path) {
    this->operand = findAndReplace(operand, func, path);
}

template <typename View, typename OperandView, typename Derived>
std::pair<bool, std::shared_ptr<Derived>>
SimpleUnaryOperator<View, OperandView, Derived>::standardOptimise(
    ExprRef<View>&, PathExtension& path) {
    auto newOp = std::make_shared<Derived>(operand);
    derived().copy(*newOp);
    bool optimised = false;
    optimised |= optimise(ExprRef<View>(newOp), newOp->operand, path);
    return std::make_pair(optimised, newOp);
}

// this assumes that Derived can be constructed with member operand, if we ever
// get to a point that one of them cannot, use the arrow return type trick to
// disable this function. i.e. use auto ...::optimise(...)  ->
// decltype(std::make_shared<Derived>) instead of
template <typename View, typename OperandView, typename Derived>
std::pair<bool, ExprRef<View>>
SimpleUnaryOperator<View, OperandView, Derived>::optimiseImpl(
    ExprRef<View>& self, PathExtension path) {
    return standardOptimise(self, path);
}

template <typename View, typename OperandView, typename Derived>
void SimpleUnaryOperator<View, OperandView,
                         Derived>::standardSanityDefinednessChecks() const {
    if (!operand->getViewIfDefined() && this->appearsDefined()) {
        throw SanityCheckException(
            "Operand is undefined but operator is still defined.");
    } else if (operand->getViewIfDefined() && !this->appearsDefined()) {
        throw SanityCheckException("operand is defined but operator is not.");
    }
}

#endif /* SRC_OPERATORS_SIMPLEOPERATOR_HPP_ */
