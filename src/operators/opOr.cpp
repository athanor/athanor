#include "operators/opOr.h"
#include <algorithm>
#include <cassert>
#include "operators/shiftViolatingIndices.h"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger = OpOr::OperandsSequenceTrigger;
UInt LARGE_VIOLATION = ((UInt)1) << ((sizeof(UInt) * 4) - 1);
UInt findNewMinViolation(OpOr& op, UInt minViolation);
inline void reevaluate(OpOr& op) {
    op.minViolationIndices.clear();
    if (op.operands->view().numberElements() == 0) {
        op.violation = LARGE_VIOLATION;
    } else {
        op.violation =
            op.operands->view().getMembers<BoolView>()[0]->view().violation;
        op.minViolationIndices.insert(0);
        op.violation = findNewMinViolation(op, op.violation);
    }
}

inline UInt findNewMinViolation(OpOr& op, UInt minViolation) {
    auto& members = op.operands->view().getMembers<BoolView>();
    for (size_t i = 0; i < members.size(); ++i) {
        auto& operand = members[i];
        UInt violation = operand->view().violation;
        if (violation < minViolation) {
            minViolation = violation;
            op.minViolationIndices.clear();
            op.minViolationIndices.insert(i);
        } else if (violation == minViolation) {
            op.minViolationIndices.insert(i);
        }
    }
    return minViolation;
}

// returns true if the function did a full revaluate of the OpOr node
inline bool handleOperandValueChange(OpOr& op, UInt index) {
    const ExprRef<BoolView> expr =
        op.operands->view().getMembers<BoolView>()[index];
    bool fullReevaluate = false;
    if (expr->view().violation < op.violation) {
        op.violation = expr->view().violation;
        op.minViolationIndices.clear();
        op.minViolationIndices.insert(index);
    } else if (expr->view().violation == op.violation) {
        op.minViolationIndices.insert(index);
    } else {
        // otherwise violation is greater, needs to be removed
        op.minViolationIndices.erase(index);
        if (op.minViolationIndices.size() == 0) {
            // new min needs to be found
            fullReevaluate = true;
            reevaluate(op);
        }
    }
    return fullReevaluate;
}

class OpOr::OperandsSequenceTrigger : public SequenceTrigger {
   public:
    OpOr* op;
    OperandsSequenceTrigger(OpOr* op) : op(op) {}
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        auto& expr = mpark::get<ExprRef<BoolView>>(exprIn);
        if (expr->view().violation > op->violation) {
            shiftIndicesUp(index, op->operands->view().numberElements(),
                           op->minViolationIndices);
            return;
        } else if (expr->view().violation < op->violation) {
            op->minViolationIndices.clear();
            op->minViolationIndices.insert(index);
            op->changeValue([&]() {
                op->violation = expr->view().violation;
                return true;
            });
            return;
        } else {
            // violation is equal to min violation
            // need to add this index to set of min violation indices.
            // must shift other indices up
            shiftIndicesUp(index, op->operands->view().numberElements(),
                           op->minViolationIndices);
            op->minViolationIndices.insert(index);
        }
    }

    void valueRemoved(UInt index, const AnyExprRef&) final {
        if (op->minViolationIndices.count(index)) {
            op->minViolationIndices.erase(index);
        }
        if (op->minViolationIndices.size() > 0) {
            shiftIndicesDown(index, op->operands->view().numberElements(),
                             op->minViolationIndices);
        } else {
            op->changeValue([&]() {
                reevaluate(*op);
                return true;
            });
        }
    }

    inline void beginSwaps() final {}
    inline void endSwaps() final {}
    inline void positionsSwapped(UInt index1, UInt index2) {
        if (op->minViolationIndices.count(index1)) {
            if (!op->minViolationIndices.count(index2)) {
                op->minViolationIndices.erase(index1);
                op->minViolationIndices.insert(index2);
            }
        } else {
            if (op->minViolationIndices.count(index2)) {
                op->minViolationIndices.erase(index2);
                op->minViolationIndices.insert(index1);
            }
        }
    }
    inline void possibleSubsequenceChange(UInt, UInt) final {}
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        op->changeValue([&]() {
            for (size_t i = startIndex; i < endIndex; i++) {
                bool fullReevaluated = handleOperandValueChange(*op, i);
                if (fullReevaluated) {
                    break;
                }
            }
            return true;
        });
    }
    void possibleValueChange() final {}
    void valueChanged() final {
        op->changeValue([&]() {
            reevaluate(*op);
            return true;
        });
    }
};

void OpOr::evaluate() {
    operands->evaluate();
    reevaluate(*this);
}

OpOr::OpOr(OpOr&& other)
    : BoolView(move(other)),
      operands(move(other.operands)),
      minViolationIndices(move(other.minViolationIndices)),
      operandsSequenceTrigger(move(other.operandsSequenceTrigger)) {
    setTriggerParent(this, operandsSequenceTrigger);
}

void OpOr::startTriggering() {
    if (!operandsSequenceTrigger) {
        operandsSequenceTrigger =
            std::make_shared<OperandsSequenceTrigger>(this);
        operands->addTrigger(operandsSequenceTrigger);
        operands->startTriggering();
    }
}

void OpOr::stopTriggeringOnChildren() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
    }
}

void OpOr::stopTriggering() {
    if (operandsSequenceTrigger) {
        deleteTrigger(operandsSequenceTrigger);
        operandsSequenceTrigger = nullptr;
        operands->stopTriggering();
    }
}

void OpOr::updateViolationDescription(const UInt,
                                      ViolationDescription& vioDesc) {
    for (auto& operand : operands->view().getMembers<BoolView>()) {
        operand->updateViolationDescription(violation, vioDesc);
    }
}

ExprRef<BoolView> OpOr::deepCopySelfForUnroll(
    const ExprRef<BoolView>&, const AnyIterRef& iterator) const {
    auto newOpOr =
        make_shared<OpOr>(operands->deepCopySelfForUnroll(operands, iterator));
    newOpOr->violation = violation;
    newOpOr->minViolationIndices = minViolationIndices;
    return newOpOr;
}

std::ostream& OpOr::dumpState(std::ostream& os) const {
    os << "OpOr: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(minViolationIndices.begin(),
                                         minViolationIndices.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Min violating indices: " << sortedViolatingOperands << endl;
    return operands->dumpState(os);
}

void OpOr::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->operands = findAndReplace(operands, func);
}
