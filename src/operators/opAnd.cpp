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
        shiftIndicesUp(index, op->operand->view()->numberElements(),
                       op->violatingOperands);
        UInt violation = expr->view()->violation;
        op->cachedViolations.insert(index, violation);
        if (violation > 0) {
            op->violatingOperands.insert(index);
            op->changeValue([&]() {
                op->violation += violation;
                return true;
            });
        }
    }

    void valueRemoved(UInt index, const AnyExprRef&) final {
        UInt violationOfRemovedExpr = op->cachedViolations.get(index);
        debug_code(assert((op->violatingOperands.count(index) &&
                           violationOfRemovedExpr > 0) ||
                          (!op->violatingOperands.count(index) &&
                           violationOfRemovedExpr == 0)));
        op->violatingOperands.erase(index);
        op->cachedViolations.erase(index);
        shiftIndicesDown(index, op->operand->view()->numberElements(),
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
        auto& members = op->operand->view()->getMembers<BoolView>();
        debug_code(assert(index < members.size()));
        return members[index]->view()->violation;
    }
    void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        UInt violationToAdd = 0, violationToRemove = 0;
        for (size_t i = startIndex; i < endIndex; i++) {
            UInt newViolation = getViolation(i);
            UInt oldViolation = op->cachedViolations.getAndSet(i, newViolation);
            violationToAdd += newViolation;
            violationToRemove += oldViolation;
            if (oldViolation > 0 && newViolation == 0) {
                op->violatingOperands.erase(i);
            } else if (oldViolation == 0 && newViolation > 0) {
                op->violatingOperands.insert(i);
            }
        }
        op->changeValue([&]() {
            op->violation -= violationToRemove;
            op->violation += violationToAdd;
            return true;
        });
    }
    void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate(true, true);
            return true;
        });
    }

    void reattachTrigger() final {
        deleteTrigger(op->operandTrigger);
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }

    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
    void memberHasBecomeUndefined(UInt) final { shouldNotBeCalledPanic; }
    void memberHasBecomeDefined(UInt) final { shouldNotBeCalledPanic; }
};

void OpAnd::reevaluateImpl(SequenceView& operandView) {
    violation = 0;
    cachedViolations.clear();
    for (size_t i = 0; i < operandView.numberElements(); ++i) {
        auto& operandChild = operand->view()->getMembers<BoolView>()[i];
        UInt operandViolation = operandChild->view()->violation;
        cachedViolations.insert(i, operandViolation);
        if (operandViolation > 0) {
            violatingOperands.insert(i);
        }
        violation += operandViolation;
    }
}

void OpAnd::updateVarViolationsImpl(const ViolationContext& vioContext,
                                    ViolationContainer& vioContainer) {
    auto* boolVioContextTest =
        dynamic_cast<const BoolViolationContext*>(&vioContext);
    bool negated = boolVioContextTest && boolVioContextTest->negated;
    if (negated) {
        if (violation == 0) {
            for (auto& operand : operand->view()->getMembers<BoolView>()) {
                operand->updateVarViolations(vioContext, vioContainer);
            }
        }
        return;
    }
    for (size_t violatingOperandIndex : violatingOperands) {
        operand->view()
            .get()
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

bool OpAnd::optimiseImpl(const PathExtension&) {
    return flatten<BoolView>(*this);
}

string OpAnd::getOpName() const { return "OpAnd"; }
void OpAnd::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    auto view = operand->getViewIfDefined();
    if (!view) {
        sanityLargeViolationCheck(violation);
        return;
    }
    auto& operandView = *view;
    UInt calcViolation = 0;
    for (auto& member : operandView.getMembers<BoolView>()) {
        auto memberView = member->getViewIfDefined();
        sanityCheck(memberView,
                    "View should not be undefined, it is a bool view.");
        calcViolation += memberView->violation;
    }
    sanityEqualsCheck(calcViolation, violation);
    ;
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAnd> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};

ExprRef<BoolView> OpMaker<OpAnd>::make(ExprRef<SequenceView> o) {
    return make_shared<OpAnd>(move(o));
}

template struct SimpleUnaryOperator<BoolView, SequenceView, OpAnd>;
