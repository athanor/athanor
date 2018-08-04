#define fudge
#ifndef fudge
#include "operators/opAllDiff.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/flatten.h"
#include "operators/previousValueCache.h"
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger =
    OperatorTrates<OpAllDiff>::OperandsSequenceTrigger;
class OperatorTrates<OpAllDiff>::OperandsSequenceTrigger
    : public SequenceTrigger {
    vector<HashType> previousHashes;
    OpAllDiff* op;
    OperandsSequenceTrigger(OpAllDiff* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        HashType newMemberHash = getValueHash(exprIn);
        previousHashes.insert(index, newMemberHash);
        if () }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        previousViolations.clear();
        const auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        UInt violationOfRemovedExpr = expr->view().violation;
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        op->violatingOperands.erase(index);
        shiftIndicesDown(index, op->operand->view().numberElements(),
                         op->violatingOperands);
        op->changeValue([&]() {
            op->violation -= violationOfRemovedExpr;
            return true;
        });
    }

    inline void beginSwaps() final { previousViolations.clear(); }
    inline void endSwaps() final { previousViolations.clear(); }
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
    }
    inline void possibleSubsequenceChange(UInt startIndex,
                                          UInt endIndex) final {
        if (endIndex - startIndex == 1) {
            previousViolations.store(startIndex,
                                     op->operand->view()
                                         .getMembers<BoolView>()[startIndex]
                                         ->view()
                                         .violation);
            return;
        }
        previousViolation = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            previousViolation +=
                op->operand->view().getMembers<BoolView>()[i]->view().violation;
        }
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        UInt newViolation = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt operandViolation =
                op->operand->view().getMembers<BoolView>()[i]->view().violation;
            newViolation += operandViolation;
            if (operandViolation > 0) {
                op->violatingOperands.insert(i);
            } else {
                op->violatingOperands.erase(i);
            }
        }
        UInt violationToRemove;
        if (endIndex - startIndex == 1) {
            violationToRemove =
                previousViolations.getAllDiffSet(startIndex, newViolation);
        } else {
            violationToRemove = previousViolation;
        }

        op->changeValue([&]() {
            op->violation -= violationToRemove;
            op->violation += newViolation;
            return true;
        });
    }

    void possibleValueChange() final {}
    void valueChanged() final {
        previousViolations.clear();
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

    void hasBecomeUndefined() final {
        previousViolations.clear();
        op->setDefined(false, true);
    }
    void hasBecomeDefined() final { op->setDefined(true, true); }
    void memberHasBecomeUndefined(UInt) final { shouldNotBeCalledPanic; }
    void memberHasBecomeDefined(UInt) final { shouldNotBeCalledPanic; }
};

void OpAllDiff::reevaluate() {
    violation = 0;
    for (size_t i = 0; i < operand->view().numberElements(); ++i) {
        auto& operandChild = operand->view().getMembers<BoolView>()[i];
        if (operandChild->view().violation > 0) {
            violatingOperands.insert(i);
        }
        violation += operandChild->view().violation;
    }
}

void OpAllDiff::updateVarViolations(const ViolationContext&,
                                    ViolationContainer& vioDesc) {
    for (size_t violatingOperandIndex : violatingOperands) {
        operand->view()
            .getMembers<BoolView>()[violatingOperandIndex]
            ->updateVarViolations(violation, vioDesc);
    }
}

void OpAllDiff::copy(OpAllDiff& newOp) const {
    newOp.violation = violation;
    newOp.violatingOperands = violatingOperands;
}

std::ostream& OpAllDiff::dumpState(std::ostream& os) const {
    os << "OpAllDiff: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                         violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
    return operand->dumpState(os);
}

bool OpAllDiff::optimiseImpl() { return flatten<BoolView>(*this); }

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAllDiff> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};

ExprRef<BoolView> OpMaker<OpAllDiff>::make(ExprRef<SequenceView> o) {
    return make_shared<OpAllDiff>(move(o));
}

void initialiseOpAllDiff(OpAllDiff&) {}

template struct SimpleUnaryOperator<BoolView, SequenceView, OpAllDiff>;

#endif
