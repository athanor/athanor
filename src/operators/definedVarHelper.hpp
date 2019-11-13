#ifndef SRC_OPERATORS_definedVARHELPER_HPP_
#define SRC_OPERATORS_definedVARHELPER_HPP_
#include "base/base.h"
#include "operators/definedVarHelper.h"
#include "operators/opAnd.h"

void handleDefinedVarTriggers();
bool isInContainerSupportingDefinedVars(ValBase* container);
void updateParentContainer(ValBase* container);

enum class DefinedDirection { NONE, LEFT, RIGHT, BOTH };

inline std::ostream& operator<<(std::ostream& os, DefinedDirection direction) {
    switch (direction) {
        case DefinedDirection::BOTH:
            os << "BOTH";
            break;
        case DefinedDirection::LEFT:
            os << "LEFT";
            break;
        case DefinedDirection::RIGHT:
            os << "RIGHT";
            break;
        case DefinedDirection::NONE:
            os << "NONE";
            break;
    }
    return os;
}

template <typename View>
inline DefinedDirection getDefinedDirection(View& leftView, View& rightView,
                                            bool leftChanged,
                                            bool rightChanged) {
    typedef typename AssociatedValueType<View>::type Value;

    auto* rightVal = dynamic_cast<Value*>(&rightView);
    bool canPropagateRight =
        leftChanged && rightVal && !rightVal->isConstant() &&
        isInContainerSupportingDefinedVars(rightVal->container) &&
        rightVal->domainContainsValue(leftView);
    auto* leftVal = dynamic_cast<Value*>(&leftView);
    bool canPropagateLeft =
        rightChanged && leftVal && !leftVal->isConstant() &&
        isInContainerSupportingDefinedVars(leftVal->container) &&
        leftVal->domainContainsValue(rightView);
    if (!canPropagateRight && !canPropagateLeft) {
        return DefinedDirection::NONE;
    }
    if (canPropagateLeft && canPropagateRight) {
        return DefinedDirection::BOTH;
    } else if (canPropagateLeft) {
        return DefinedDirection::LEFT;
    } else {
        return DefinedDirection::RIGHT;
    }
}

template <typename Op>
struct DefinedVarTrigger;
struct OpIntEq;
struct OpBoolEq;
struct OpEnumEq;
template <typename Op>
struct DefinedVarTrigger {
    bool _active = true;
    Op* op;
    DefinedDirection definedDirection;
    DefinedVarTrigger(Op* op, DefinedDirection definedDirection)
        : op(op), definedDirection(definedDirection) {}

    inline bool& active() { return _active; }
    void trigger() {
        active() = false;
        op->definedVarTrigger = NULL;

        op->definesLock.tryLock();
        auto leftOption = op->left->getViewIfDefined();
        auto rightOption = op->right->getViewIfDefined();
        auto& fromOption = (definedDirection == DefinedDirection::RIGHT)
                               ? leftOption
                               : rightOption;
        auto& toOption = (definedDirection == DefinedDirection::RIGHT)
                             ? rightOption
                             : leftOption;

        typedef BaseType<decltype(*fromOption)> View;
        typedef typename AssociatedValueType<View>::type Value;
        if (!fromOption || !toOption) {
            // if operands have become undefined
            op->setUndefinedAndTrigger();
        } else if (!dynamic_cast<Value*>(&toOption.get())
                        ->domainContainsValue(*fromOption)) {
            // cannot propagate in this direction as value is not in domain.
            // Tell Op to reevaluate itself.
            op->updateValue(*op->left->view(), *op->right->view());
        } else {
            toOption->matchValueOf(*fromOption);
            auto& val = static_cast<Value&>(*toOption);
            updateParentContainer(val.container);
            // must call updateValue to the operator as if the value was already
            // matched, the op might not know this and may not have updated
            op->updateValue(*leftOption, *rightOption);
        }
    }
};

typedef lib::variant<DefinedVarTrigger<OpIntEq>, DefinedVarTrigger<OpBoolEq>,
                     DefinedVarTrigger<OpEnumEq>>
    AnyDefinedVarTrigger;

extern std::deque<AnyDefinedVarTrigger> definedVarTriggerQueue;
extern std::deque<AnyDefinedVarTrigger> delayedDefinedVarTriggerQueue;

template <typename Op>
DefinedVarTrigger<Op>* addDefinedVarTrigger(Op* op,
                                            DefinedDirection definedDirection,
                                            bool delayedQueue) {
    auto& queue =
        (delayedQueue) ? delayedDefinedVarTriggerQueue : definedVarTriggerQueue;
    queue.emplace_back(DefinedVarTrigger<Op>(op, definedDirection));
    return &lib::get<DefinedVarTrigger<Op>>(queue.back());
}

// returns true if this function promises to do the necessary steps to correctly
// update the value of the op, otherwise returns false to signify to the op that
// it needs  to update its value.
template <typename Op, typename View>
inline void handledByForwardingValueChange(Op& op, View& leftView,
                                           View& rightView, bool leftChanged,
                                           bool rightChanged) {
    // check unnecessary for correctness, but allows quicker fail
    if (!op.definedVarTrigger && !op.definesLock.softTry()) {
        op.updateValue(leftView, rightView);
        return;
    }

    auto definedDir =
        getDefinedDirection(leftView, rightView, leftChanged, rightChanged);
    if (definedDir == DefinedDirection::NONE) {
        op.updateValue(leftView, rightView);
        return;
    }
    if (op.definedVarTrigger) {
        // We already queued this op for propagating
        if (op.definedVarTrigger->definedDirection == DefinedDirection::BOTH) {
            // the direction of this propagation was previously unknown. now a
            // direction might beenbe known, update it.
            op.definedVarTrigger->definedDirection = definedDir;
        }
        return;
    }

    if (leftChanged && rightChanged) {
        // when both sides are marked as being changed, the op is either just
        // become defined or is being evaluated for the first time. Add it to
        // the delayed queue for propagation but still update the value of the
        // op.
        op.definedVarTrigger = addDefinedVarTrigger(&op, definedDir, true);
        op.updateValue(leftView, rightView);
    } else {
        // only one side changed, queue a trigger.
        op.definedVarTrigger = addDefinedVarTrigger(&op, definedDir, false);
    }
}

// check if path up to root consists of only OpAnd and sequence view operators
// and that the current expression is not under a quantifier condition,
inline bool isSuitableForDefiningVars(const PathExtension& path) {
    const PathExtension* current = &path;
    // path does not include self, only parents
    bool success = !path.isCondition;
    while (success && !current->isTop()) {
        lib::visit(
            [&](auto& expr) {
                if (!getAs<OpAnd>(expr) &&
                    !std::is_same<SequenceView, viewType(expr)>::value) {
                    success = false;
                }
                current = current->parent;
            },
            current->expr);
    }
    return success;
}
#endif /* SRC_OPERATORS_definedVARHELPER_HPP_ */
