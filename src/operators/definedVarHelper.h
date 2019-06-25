#ifndef SRC_OPERATORS_definedVARHELPER_H_
#define SRC_OPERATORS_definedVARHELPER_H_
#include "base/base.h"
#include "operators/opAnd.h"

// lock structure, used to prevent circular walks when expressions forward
// values to variables that are defined off them.
class DefinesLock {
    static u_int64_t globalStamp;
    u_int64_t localStamp = 0;

   public:
    // try to acquire this lock, this will only return  true once  per round of
    // triggering.  On the next round of triggering another call will be
    // allowed.
    inline bool tryLock() {
        if (localStamp >= globalStamp) {
            return false;
        }
        localStamp = globalStamp;
        return true;
    }
    // returns whether or not try() will return true, but unlike try() the state
    // does not get changed. i.e. softTry() can be queried more than once per
    // triggering round.
    inline bool softTry() { return localStamp < globalStamp; }

    // disable this lock so that try() always returns false
    inline void disable() {
        localStamp = std::numeric_limits<u_int64_t>().max();
    }

    inline bool isDisabled() const {
        return localStamp == std::numeric_limits<u_int64_t>().max();
    }
    // reset this lock, allow try() to return true again
    void reset() { localStamp = 0; }

    // unlock all locks signalling the next round of triggering.  Those who
    // called tryLock() will be  able to call tryLock() again.  Those with
    // disabled locks will still not be able to call tryLock() until reset() has
    // been called on them.
    static void unlockAll() { ++globalStamp; }
};

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
    bool canPropagateRight = leftChanged && rightVal &&
                             !rightVal->isConstant() &&
                             rightVal->domainContainsValue(leftView);
    auto* leftVal = dynamic_cast<Value*>(&leftView);
    bool canPropagateLeft = rightChanged && leftVal && !leftVal->isConstant() &&
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
            // must call updateValue to the operator as if the value was already
            // matched, the op might not know this and may not have updated
            op->updateValue(*leftOption, *rightOption);
        }
    }
};

typedef mpark::variant<DefinedVarTrigger<OpIntEq>, DefinedVarTrigger<OpBoolEq>,
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
    return &mpark::get<DefinedVarTrigger<Op>>(queue.back());
}

// returns true if this function promises to do the necessary steps to correctly
// update the value of the op, otherwise returns false to signify to the op that
// it needs  to update its value.
template <typename Op, typename View>
inline void handledByForwardingValueChange(Op& op, View& leftView,
                                           View& rightView, bool leftChanged,
                                           bool rightChanged) {
    //check unnecessary for correctness, but allows quicker fail
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

void handleDefinedVarTriggers();

// check if path up to root consists of only OpAnd and sequence view operators
// and that the current expression is not under a quantifier condition,
inline bool isSuitableForDefiningVars(const PathExtension& path) {
    PathExtension* current = path.parent;
    bool success = !path.isCondition;
    while (current && success) {
        mpark::visit(
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
#endif /* SRC_OPERATORS_definedVARHELPER_H_ */
