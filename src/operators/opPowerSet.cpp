
// this operator is no longer intended to be part of the AST during search. The
// parser should detect it and re-write it into a different expression.
#include "operators/opPowerSet.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/set.h"

using namespace std;

void OpPowerSet::reevaluateImpl(SetView&) { todoImpl(); }

struct OperatorTrates<OpPowerSet>::OperandTrigger
    : public ChangeTriggerAdapter<SetTrigger,
                                  OperatorTrates<OpPowerSet>::OperandTrigger> {
    OpPowerSet* op;
    OperandTrigger(OpPowerSet* op)
        : ChangeTriggerAdapter<SetTrigger,
                               OperatorTrates<OpPowerSet>::OperandTrigger>(
              op->operand),
          op(op) {}

    inline void adapterValueChanged() { todoImpl(); }

    void reattachTrigger() {
        deleteTrigger(op->operandTrigger);
        auto newTrigger =
            std::make_shared<OperatorTrates<OpPowerSet>::OperandTrigger>(op);
        op->operand->addTrigger(newTrigger);
        op->operandTrigger = newTrigger;
    }

    void adapterHasBecomeUndefined() { todoImpl(); }

    void adapterHasBecomeDefined() { todoImpl(); }
};

void OpPowerSet::updateVarViolationsImpl(const ViolationContext&,
                                         ViolationContainer&) {
    todoImpl();
}

void OpPowerSet::copy(OpPowerSet&) const {}

std::ostream& OpPowerSet::dumpState(std::ostream& os) const {
    os << "OpPowerSet:\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpPowerSet::getOpName() const { return "OpPowerSet"; }
void OpPowerSet::debugSanityCheckImpl() const { todoImpl(); }

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpPowerSet> {
    static ExprRef<SetView> make(ExprRef<SetView> o);
};

ExprRef<SetView> OpMaker<OpPowerSet>::make(ExprRef<SetView> o) {
    return make_shared<OpPowerSet>(move(o));
}
