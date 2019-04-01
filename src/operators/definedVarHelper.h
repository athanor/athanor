#ifndef SRC_OPERATORS_definedVARHELPER_H_
#define SRC_OPERATORS_definedVARHELPER_H_
#include "base/base.h"

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
    inline bool softTry() {
        return localStamp < globalStamp;
    }

    // disable this lock so that try() always returns false
    void disable() { localStamp = std::numeric_limits<u_int64_t>().max(); }

    // reset this lock, allow try() to return true again
    void reset() { localStamp = 0; }

   public:
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
                             rightVal->domainContainsValue(leftView.value);
    auto* leftVal = dynamic_cast<Value*>(&leftView);
    bool canPropagateLeft = rightChanged && leftVal && !leftVal->isConstant() &&
                            leftVal->domainContainsValue(rightView.value);
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

        debug_code(op->dumpState(std::cout) << std::endl);
        debug_log("triggering: direction is " << definedDirection);
        op->definesLock.tryLock();
        auto& from = (definedDirection == DefinedDirection::RIGHT) ? op->left
                                                                   : op->right;
        auto& to = (definedDirection == DefinedDirection::RIGHT) ? op->right
                                                                 : op->left;
        debug_log("from=" << from << ", to=" << to);
        auto fromOption = from->getViewIfDefined();
        auto toOption = to->getViewIfDefined();
        typedef BaseType<decltype(*fromOption)> View;
        typedef typename AssociatedValueType<View>::type Value;
        if (!fromOption || !toOption) {
            debug_log("undefined");
            // if operands have become undefined
            op->setUndefinedAndTrigger();
        } else if (!dynamic_cast<Value*>(&toOption.get())
                        ->domainContainsValue(fromOption->value)) {
            debug_log("not in domain, updating value");
            // cannot propagate in this direction as value is not in domain.
            // Tell Op to reevaluate itself.
            op->updateValue(*op->left->view(), *op->right->view());
        } else {
            toOption->matchValueOf(*fromOption);
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
    debug_log("checking");
    debug_code(op.dumpState(std::cout) << std::endl);
    auto definedDir = getDefinedDirection(leftView, rightView,
                                          leftChanged, rightChanged);
    debug_log("direction is " << definedDir);
    if (definedDir == DefinedDirection::NONE) {
        debug_log("updating value");
        op.updateValue(leftView, rightView);
        return;
    }
    if (op.definedVarTrigger) {
        debug_log("already queued this op for propagating");
        // We already queued this op for propagating
        if (op.definedVarTrigger->definedDirection == DefinedDirection::BOTH) {
            // the direction of this propagation was previously unknown. now a
            // direction might beenbe known, update it.
            debug_log("changed the direction from both to " << definedDir);
            op.definedVarTrigger->definedDirection = definedDir;
        }
        return;
    }

    if (leftChanged && rightChanged) {
        debug_log("both sides changed, queueing to delayed");
        // when both sides are marked as being changed, the op is either just
        // become defined or is being evaluated for the first time. Add it to
        // the delayed queue for propagation but tell the op (by returning
        // false) that it must still update its value.

        op.definedVarTrigger = addDefinedVarTrigger(&op, definedDir, true);
        debug_log("but also updating value");
        op.updateValue(leftView, rightView);
    } else {
        debug_log("only one side changed");
        // only one side changed, queue it only if it is not locked, the locks
        // should stop circular infinite walks.
        if (op.definesLock.softTry()) {
            debug_log("soft lock");
            op.definedVarTrigger = addDefinedVarTrigger(&op, definedDir, false);
        } else {
            debug_log("did not get lock, updating value");
            op.updateValue(leftView, rightView);
            ;
        }
    }
}

void handleDefinedVarTriggers();
#endif /* SRC_OPERATORS_definedVARHELPER_H_ */
