#ifndef SRC_OPERATORS_QUANTIFIERTRIGGERS_HPP_
#define SRC_OPERATORS_QUANTIFIERTRIGGERS_HPP_
#include "operators/quantifier.h"
#include "triggers/allTriggers.h"

template <typename ContainerType, typename View>
struct ExprChangeTrigger
    : public ChangeTriggerAdapter<typename AssociatedTriggerType<View>::type,
                                  ExprChangeTrigger<ContainerType, View>>,
      public Quantifier<ContainerType>::ExprTriggerBase {
    using Quantifier<ContainerType>::ExprTriggerBase::op;
    using Quantifier<ContainerType>::ExprTriggerBase::index;
    typedef typename AssociatedTriggerType<View>::type TriggerType;

    ExprChangeTrigger(Quantifier<ContainerType>* op, UInt index)
        : ChangeTriggerAdapter<TriggerType,
                               ExprChangeTrigger<ContainerType, View>>(
              op->template getMembers<View>()[index]),
          Quantifier<ContainerType>::ExprTriggerBase(op, index) {}

    ExprRef<View>& getTriggeringOperand() {
        return op->template getMembers<View>()[index];
    }
    void adapterValueChanged() {
        lib::visit(
            [&](auto& members) {
                op->template changeSubsequenceAndNotify<viewType(members)>(
                    index, index + 1);
            },
            op->members);
    }
    void reattachTrigger() final {
        auto& triggerToChange = op->exprTriggers.at(index);
        deleteTrigger(
            std::static_pointer_cast<ExprChangeTrigger<ContainerType, View>>(
                triggerToChange));
        auto trigger =
            std::make_shared<ExprChangeTrigger<ContainerType, View>>(op, index);
        op->template getMembers<View>()[index]->addTrigger(trigger);
        triggerToChange = trigger;
    }

    void adapterHasBecomeDefined() {
        lib::visit(
            [&](auto& members) {
                op->template defineMemberAndNotify<viewType(members)>(index);
            },
            op->members);
    }
    void adapterHasBecomeUndefined() {
        lib::visit(
            [&](auto& members) {
                op->template undefineMemberAndNotify<viewType(members)>(index);
            },
            op->members);
    }
};
template <typename ContainerType>
struct ConditionChangeTrigger
    : public ChangeTriggerAdapter<BoolTrigger,
                                  ConditionChangeTrigger<ContainerType>>,
      public Quantifier<ContainerType>::ExprTriggerBase {
    using Quantifier<ContainerType>::ExprTriggerBase::op;
    using Quantifier<ContainerType>::ExprTriggerBase::index;
    ConditionChangeTrigger(Quantifier<ContainerType>* op, UInt index)
        : ChangeTriggerAdapter<BoolTrigger,
                               ConditionChangeTrigger<ContainerType>>(
              op->unrolledConditions[index].condition),
          Quantifier<ContainerType>::ExprTriggerBase(op, index) {}

    ExprRef<BoolView>& getTriggeringOperand() {
        return op->unrolledConditions[index].condition;
    }
    void adapterValueChanged() {
        auto& unrolledConditions = op->unrolledConditions;
        bool oldValue = unrolledConditions[index].cachedValue;
        unrolledConditions[index].cachedValue =
            unrolledConditions[index].condition->view()->violation == 0;
        bool newValue = op->unrolledConditions[index].cachedValue;
        if (oldValue == newValue) {
            return;
        } else if (!oldValue && newValue) {
            handleConditionBecomingTrue();
        } else if (oldValue && !newValue) {
            handleConditionBecomingFalse();
        }
    }

    void handleConditionBecomingTrue() {
        debug_code(assert(index < op->unrolledIterVals.size()));
        auto& unrolledConditions = op->unrolledConditions;
        std::for_each(unrolledConditions.begin() + index,
                      unrolledConditions.end(),
                      [&](auto& cond) { ++cond.exprIndex; });
        lib::visit(
            [&](auto& unrolledIterVal) {
                typedef viewType(unrolledIterVal->getValue()) View;
                op->template unroll<View>({true,
                                           unrolledConditions[index].exprIndex,
                                           unrolledIterVal->getValue()});
            },
            op->unrolledIterVals[index]);
    }
    void handleConditionBecomingFalse() {
        auto& unrolledConditions = op->unrolledConditions;
        op->rollExpr(unrolledConditions[index].exprIndex);
        std::for_each(unrolledConditions.begin() + index,
                      unrolledConditions.end(),
                      [&](auto& cond) { --cond.exprIndex; });
    }

    void reattachTrigger() final {
        auto& triggerToChange = op->unrolledConditions.at(index).trigger;
        deleteTrigger(
            std::static_pointer_cast<ConditionChangeTrigger<ContainerType>>(
                triggerToChange));
        auto trigger =
            std::make_shared<ConditionChangeTrigger<ContainerType>>(op, index);
        getTriggeringOperand()->addTrigger(trigger);
        triggerToChange = trigger;
    }

    void adapterHasBecomeDefined() { shouldNotBeCalledPanic; }
    void adapterHasBecomeUndefined() { shouldNotBeCalledPanic; }
};
#endif /* SRC_OPERATORS_QUANTIFIERTRIGGERS_HPP_ */
