
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
    SimpleBinaryTrigger(Op* op) : op(op) {}
    ExprRef<OperandType>& getTriggeringOperand() {
        return (isLeftTrigger) ? op->left : op->right;
    }
    inline void adapterValueChanged() {
        if (!op->isDefined() || !op->allOperandsAreDefined()) {
            return;
        }
        op->changeValue([&]() {
            bool defined = op->isDefined();
            op->reevaluate();
            return op->isDefined() != defined;
        });
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

    void adapterHasBecomeUndefined() { op->setDefined(false, true); }

    void adapterHasBecomeDefined() {
        if ((isLeftTrigger && op->right->appearsDefined()) ||
            (!isLeftTrigger && op->left->appearsDefined())) {
            op->setDefined(true, true);
        }
    }
};

template <typename Op, typename TriggerType>
struct SimpleUnaryTrigger
    : public ChangeTriggerAdapter<TriggerType,
                                  SimpleUnaryTrigger<Op, TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type OperandType;
    Op* op;
    SimpleUnaryTrigger(Op* op) : op(op) {}

    ExprRef<OperandType>& getTriggeringOperand() { return op->operand; }
    inline void adapterValueChanged() {
        op->changeValue([&]() {
            bool defined = op->isDefined();
            op->reevaluate();
            return defined != op->isDefined();
        });
    }

    void reattachTrigger() {
        deleteTrigger(op->operandTrigger);
        auto newTrigger =
            std::make_shared<SimpleUnaryTrigger<Op, TriggerType>>(op);
        op->operand->addTrigger(newTrigger);
        op->operandTrigger = newTrigger;
    }

    void adapterHasBecomeUndefined() { op->setDefined(false, true); }

    void adapterHasBecomeDefined() { op->setDefined(true, true); }
};

#endif /* SRC_OPERATORS_SIMPLETRIGGER_H_ */
