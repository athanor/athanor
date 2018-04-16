
#ifndef SRC_OPERATORS_SIMPLETRIGGER_H_
#define SRC_OPERATORS_SIMPLETRIGGER_H_
#include <memory>
#include "base/base.h"
template <typename Op, typename TriggerType, bool isLeftTrigger>
struct SimpleBinaryTrigger
    : public ChangeTriggerAdapter<
          TriggerType, SimpleBinaryTrigger<Op, TriggerType, isLeftTrigger>> {
    Op* op;
    SimpleBinaryTrigger(Op* op) : op(op) {}
    inline void adapterPossibleValueChange() {}
    inline void adapterValueChanged() {
        op->changeValue([&]() {
            op->reevaluate();
            return true;
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
};
template <typename Op, typename TriggerType>
struct SimpleUnaryTrigger
    : public ChangeTriggerAdapter<TriggerType,
                                  SimpleUnaryTrigger<Op, TriggerType>> {
    Op* op;
    SimpleUnaryTrigger(Op* op) : op(op) {}
    inline void adapterPossibleValueChange() {}
    inline void adapterValueChanged() {
        op->changeValue([&]() {
            op->reevaluate();
            return true;
        });
    }

    void reattachTrigger() {
        deleteTrigger(op->operandTrigger);
        auto newTrigger =
            std::make_shared<SimpleUnaryTrigger<Op, TriggerType>>(op);
        op->operand->addTrigger(newTrigger);
        op->operandTrigger = newTrigger;
    }
};

#endif /* SRC_OPERATORS_SIMPLETRIGGER_H_ */
