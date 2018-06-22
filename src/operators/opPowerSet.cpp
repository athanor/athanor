#include "operators/opPowerSet.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/set.h"
#include "utils/ignoreUnused.h"

using namespace std;
struct OpSetLit;
template <typename Op>
struct OpMaker;
template <>
struct OpMaker<OpSetLit> {
    static ExprRef<SetView> make(AnyExprVec operands);
};

void OpPowerSet::reevaluate() {
    if (sizeLimit != 2) {
        std::cerr << "Only supporting powerset when limiting produced sets "
                     "to exactly size 2.\n";
        abort();
    }

    silentClear();
    mpark::visit(
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); i++) {
                for (size_t j = i + 1; j < members.size(); j++) {
                    auto subset =
                        OpMaker<OpSetLit>::make(ExprRefVec<viewType(members)>(
                            {members[i], members[j]}));
                    subset->evaluate();
                    this->addMember(subset);
                }
            }
        },
        operand->view().members);
}

struct OperatorTrates<OpPowerSet>::OperandTrigger : public SetTrigger {
    OpPowerSet* op;
    OperandTrigger(OpPowerSet* op) : op(op) {}
    void possibleValueChange() final { op->notifyPossibleSetValueChange(); }
    void valueChanged() {
        op->reevaluate();
        op->notifyEntireSetChange();
    }

    void hasBecomeUndefined() { todoImpl(); }
    void hasBecomeDefined() { todoImpl(); }

    void valueRemoved(UInt, HashType) final {
        cerr << "Only supporting powersets over fixed size sets at the "
                "moment.\n";
        todoImpl();
    }
    void reattachTrigger() final {
        auto trigger = make_shared<OperandTrigger>(op);
        deleteTrigger(op->operandTrigger);
        this->op->operand->addTrigger(trigger);
        op->operandTrigger = move(trigger);
    }
    void valueAdded(const AnyExprRef&) final {
        cerr << "Only supporting powersets over fixed size sets at the "
                "moment.\n";
        todoImpl();
        ;
    }
    void possibleMemberValueChange(UInt, const AnyExprRef&) final {
        // ignore, ;
    }
    void memberValueChanged(UInt, const AnyExprRef&) final {
        // ignore;
    }
};
void OpPowerSet::updateVarViolations(const ViolationContext&,
                                     ViolationContainer&) {}

void OpPowerSet::copy(OpPowerSet& newOp) const {
    newOp.sizeLimit = sizeLimit;
    newOp.reevaluate();
}

std::ostream& OpPowerSet::dumpState(std::ostream& os) const {
    os << "OpPowerSet: value=";
    prettyPrint(os, this->view());
    os << "\noperand: ";
    operand->dumpState(os);
    return os;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpPowerSet> {
    static ExprRef<SetView> make(ExprRef<SetView> o);
};

ExprRef<SetView> OpMaker<OpPowerSet>::make(ExprRef<SetView> o) {
    return make_shared<OpPowerSet>(move(o));
}
