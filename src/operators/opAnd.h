
#ifndef SRC_OPERATORS_OPAND_H_
#define SRC_OPERATORS_OPAND_H_
#include <vector>

#include "operators/previousValueCache.h"
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.h"
#include "types/bool.h"
#include "types/sequence.h"
#include "utils/fastIterableIntSet.h"

struct OpAnd;
template <>
struct OperatorTrates<OpAnd> {
    class OperandsSequenceTrigger;
    typedef OperandsSequenceTrigger OperandTrigger;
};

struct OpAnd : public SimpleUnaryOperator<BoolView, SequenceView, OpAnd> {
    using SimpleUnaryOperator<BoolView, SequenceView,
                              OpAnd>::SimpleUnaryOperator;
    PreviousValueCache<UInt> cachedViolations;
    FastIterableIntSet violatingOperands = FastIterableIntSet(0, 0);

    inline OpAnd& operator=(const OpAnd& other) {
        operand = other.operand;
        violatingOperands = other.violatingOperands;
        return *this;
    }
    OpAnd(OpAnd&&) = delete;
    void reevaluateImpl(SequenceView& operandView);
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer& vioContainer) final;
    void copy(OpAnd& newOp) const;
    std::ostream& dumpState(std::ostream& os) const final;
    std::pair<bool, ExprRef<BoolView>> optimiseImpl(ExprRef<BoolView>&,
                                                    PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};

class OperatorTrates<OpAnd>::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    OpAnd* op;
    OperandsSequenceTrigger(OpAnd* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = lib::get<ExprRef<BoolView>>(exprIn);
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
        std::swap(op->cachedViolations.get(index1),
                  op->cachedViolations.get(index2));
    }

    UInt getViolation(size_t index) {
        auto& members = op->operand->view()->getMembers<BoolView>();
        debug_code(assert(index < members.size()));
        return members[index]->view()->violation;
    }
    void memberReplaced(UInt index, const AnyExprRef&) final {
        subsequenceChanged(index, index + 1);
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
        auto trigger = std::make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }

    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
    void memberHasBecomeUndefined(UInt) final { shouldNotBeCalledPanic; }
    void memberHasBecomeDefined(UInt) final { shouldNotBeCalledPanic; }
};

#endif /* SRC_OPERATORS_OPAND_H_ */
