
#ifndef SRC_OPERATORS_SIMPLETRIGGER_H_
#define SRC_OPERATORS_SIMPLETRIGGER_H_
#include <memory>
#include "base/base.h"

template <typename Op>
using IsBoolReturning = std::is_base_of<BoolView, Op>;

template <typename Op, typename TriggerType, bool isLeftTrigger>
struct SimpleBinaryTrigger
    : public ChangeTriggerAdapter<
          TriggerType, SimpleBinaryTrigger<Op, TriggerType, isLeftTrigger>> {
    typedef typename AssociatedViewType<TriggerType>::type OperandType;
    Op* op;
    SimpleBinaryTrigger(Op* op)
        : ChangeTriggerAdapter<
              TriggerType, SimpleBinaryTrigger<Op, TriggerType, isLeftTrigger>>(
              (isLeftTrigger) ? op->left : op->right),
          op(op) {}
    ExprRef<OperandType>& getTriggeringOperand() {
        return (isLeftTrigger) ? op->left : op->right;
    }
    inline void adapterValueChanged() {
        bool wasDefined = op->isDefined();
        op->changeValue([&]() {
            op->reevaluate();
            return wasDefined && op->isDefined();
        });
        if (wasDefined && !op->isDefined()) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        } else if (!wasDefined && op->isDefined()) {
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers,
                          true);
        }
    }

    inline void reattachTrigger() final {
        if (isLeftTrigger) {
            reassignLeftTrigger();
        } else {
            reassignRightTrigger();
        }
    }
    inline void reassignLeftTrigger() {
        deleteTrigger(op->leftTrigger);
        auto newTrigger =
            std::make_shared<SimpleBinaryTrigger<Op, TriggerType, true>>(op);
        op->left->addTrigger(newTrigger);
        op->leftTrigger = newTrigger;
    }
    inline void reassignRightTrigger() {
        deleteTrigger(op->rightTrigger);
        auto newTrigger =
            std::make_shared<SimpleBinaryTrigger<Op, TriggerType, false>>(op);
        op->right->addTrigger(newTrigger);
        op->rightTrigger = newTrigger;
    }

    void adapterHasBecomeUndefined() { op->setUndefinedAndTrigger(); }

    void adapterHasBecomeDefined() {
        if ((isLeftTrigger && op->right->appearsDefined()) ||
            (!isLeftTrigger && op->left->appearsDefined())) {
            op->reevaluateDefinedAndTrigger();
        }
    }
};

template <typename Op, typename TriggerType>
struct SimpleUnaryTrigger
    : public ChangeTriggerAdapter<TriggerType,
                                  SimpleUnaryTrigger<Op, TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type OperandType;
    Op* op;
    SimpleUnaryTrigger(Op* op)
        : ChangeTriggerAdapter<TriggerType,
                               SimpleUnaryTrigger<Op, TriggerType>>(
              op->operand),
          op(op) {}

    ExprRef<OperandType>& getTriggeringOperand() { return op->operand; }
    inline void adapterValueChanged() {
        bool wasDefined = op->isDefined();
        op->changeValue([&]() {
            op->reevaluate();
            return wasDefined && op->isDefined();
        });
        if (wasDefined && !op->isDefined()) {
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        } else if (!wasDefined && op->isDefined()) {
            visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers,
                          true);
        }
    }

    void reattachTrigger() {
        deleteTrigger(op->operandTrigger);
        auto newTrigger =
            std::make_shared<SimpleUnaryTrigger<Op, TriggerType>>(op);
        op->operand->addTrigger(newTrigger);
        op->operandTrigger = newTrigger;
    }

    void adapterHasBecomeUndefined() { op->setUndefinedAndTrigger(); }

    void adapterHasBecomeDefined() { op->reevaluateDefinedAndTrigger(); }
};

#endif /* SRC_OPERATORS_SIMPLETRIGGER_H_ */
