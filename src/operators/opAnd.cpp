#include "operators/opAnd.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/flatten.h"
#include "operators/previousValueCache.h"
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OperatorTrates<OpAnd>::OperandsSequenceTrigger;
class OperatorTrates<OpAnd>::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    OpAnd* op;
    OperandsSequenceTrigger(OpAnd* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        shiftIndicesUp(index, op->operand->view().numberElements(),
                       op->violatingOperands);
        UInt violation = expr->view().violation;
        op->cachedViolations.insert(index, violation);
        if (violation > 0) {
            op->violatingOperands.insert(index);
            op->changeValue([&]() {
                op->violation += violation;
                return true;
            });
        }
    }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        debug_code(const auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
                   UInt violation = expr->view().violation;
                   assert(violation == op->cachedViolations.get(index)));
        UInt violationOfRemovedExpr = op->cachedViolations.get(index);
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        op->violatingOperands.erase(index);
        op->cachedViolations.erase(index);
        shiftIndicesDown(index, op->operand->view().numberElements(),
                         op->violatingOperands);
        op->changeValue([&]() {
            op->violation -= violationOfRemovedExpr;
            return true;
        });
    }

    inline void positionsSwapped(UInt index1, UInt index2) {
        if (op->violatingOperands.count(index1)) {
            if (!op->violatingOperands.count(index2)) {
                op->violatingOperands.erase(index1);
                op->violatingOperands.insert(index2);
            }
        } else {
            if (op->violatingOperands.count(index2)) {
                op->violatingOperands.erase(index2);
                op->violatingOperands.insert(index1);
            }
        }
        swap(op->cachedViolations.get(index1),
             op->cachedViolations.get(index2));
    }

    UInt getViolation(size_t index) {
        auto& members = op->operand->view().getMembers<BoolView>();
        debug_code(assert(index < members.size()));
        return members[index]->view().violation;
    }
    void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        UInt violationToAdd = 0, violationToRemove = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt newViolation = getViolation(i);
            violationToAdd += newViolation;
            violationToRemove +=
                op->cachedViolations.getAndSet(i, newViolation);
        }
        op->changeValue([&]() {
            op->violation -= violationToRemove;
            op->violation += violationToAdd;
            return true;
        });
    }
    void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate();
            return true;
        });
    }

    void reattachTrigger() final {
        deleteTrigger(op->operandTrigger);
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }

    void hasBecomeUndefined() final { op->setDefined(false, true); }
    void hasBecomeDefined() final { op->setDefined(true, true); }
    void memberHasBecomeUndefined(UInt) final { shouldNotBeCalledPanic; }
    void memberHasBecomeDefined(UInt) final { shouldNotBeCalledPanic; }
};

void OpAnd::reevaluate() {
    violation = 0;
    cachedViolations.clear();
    for (size_t i = 0; i < operand->view().numberElements(); ++i) {
        auto& operandChild = operand->view().getMembers<BoolView>()[i];
        UInt operandViolation = operandChild->view().violation;
        cachedViolations.insert(i, operandViolation);
        if (operandViolation > 0) {
            violatingOperands.insert(i);
        }
        violation += operandViolation;
    }
}

void OpAnd::updateVarViolationsImpl(const ViolationContext&,
                                    ViolationContainer& vioContainer) {
    for (size_t violatingOperandIndex : violatingOperands) {
        operand->view()
            .getMembers<BoolView>()[violatingOperandIndex]
            ->updateVarViolations(violation, vioContainer);
    }
}

void OpAnd::copy(OpAnd& newOp) const {
    newOp.violation = violation;
    newOp.cachedViolations = cachedViolations;
    newOp.violatingOperands = violatingOperands;
}

std::ostream& OpAnd::dumpState(std::ostream& os) const {
    os << "OpAnd: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                         violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
    return operand->dumpState(os) << ")";
}

bool OpAnd::optimiseImpl() { return flatten<BoolView>(*this); }

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAnd> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};

ExprRef<BoolView> OpMaker<OpAnd>::make(ExprRef<SequenceView> o) {
    return make_shared<OpAnd>(move(o));
}

void initialiseOpAnd(OpAnd&) {}

template struct SimpleUnaryOperator<BoolView, SequenceView, OpAnd>;
