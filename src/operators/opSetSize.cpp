#include "operators/opSetSize.h"
#include <iostream>
#include <memory>
#include "types/set.h"

using namespace std;

void evaluate(OpSetSize& op) {
    evaluate(op.operand);
    op.value = getView<SetView>(op.operand).memberHashes.size();
}

class OpSetSizeTrigger : public SetTrigger {
    OpSetSize& op;

   public:
    OpSetSizeTrigger(OpSetSize& op) : op(op) {}
    inline void valueRemoved(const Value&) final {
        for (auto& trigger : op.triggers) {
            trigger->possibleValueChange(op.value);
        }
        --op.value;
        for (auto& trigger : op.triggers) {
            trigger->valueChanged(op.value);
        }
    }
    inline void valueAdded(const Value&) final {
        for (auto& trigger : op.triggers) {
            trigger->possibleValueChange(op.value);
        }
        ++op.value;
        for (auto& trigger : op.triggers) {
            trigger->valueChanged(op.value);
        }
    }
    inline void possibleValueChange(const Value&) {}
    inline void valueChanged(const Value&) {}
};

void startTriggering(OpSetSize& op) {
    getView<SetView>(op.operand)
        .triggers.emplace_back(make_shared<OpSetSizeTrigger>(op));
    startTriggering(op.operand);
}

void stopTriggering(OpSetSize& op) {
    assert(false);  // todo
    stopTriggering(op.operand);
}

void updateViolationDescription(const OpSetSize& op, u_int64_t parentViolation,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.operand, parentViolation, vioDesc);
}
