#include "operators/opSubset.h"
#include <iostream>
#include <memory>
#include "types/set.h"
#include "types/typeOperations.h"
using namespace std;

using LeftSetTrigger = OpSubset::LeftSetTrigger;
using RightSetTrigger = OpSubset::RightSetTrigger;

inline void evaluate(OpSubset& op, const SetView& leftSetView,
                     const SetView& rightSetView) {
    op.violation = 0;
    for (auto& hashIndexPair : leftSetView.getHashIndexMap()) {
        op.violation +=
            !rightSetView.getHashIndexMap().count(hashIndexPair.first);
    }
}
void evaluate(OpSubset& op) {
    evaluate(op.left);
    evaluate(op.right);
    evaluate(op, getView<SetView>(op.left), getView<SetView>(op.right));
}

struct OpSubset::LeftSetTrigger : public SetTrigger {
   public:
    OpSubset* op;
    u_int64_t oldHash;

   public:
    LeftSetTrigger(OpSubset* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t hash) final {
        if (!getView<SetView>(op->right).getHashIndexMap().count(hash)) {
            op->changeValue([&]() { --op->violation; return true; });
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        if (!getView<SetView>(op->right).getHashIndexMap().count(hash)) {
            op->changeValue([&]() { ++op->violation; return true; });
        }
    }

    inline void setValueChanged(const SetView& newValue) final {
        op->changeValue(
            [&]() { evaluate(*op, newValue, getView<SetView>(op->right)); return true; });
    }

    inline void possibleMemberValueChange(u_int64_t,
                                          const AnyValRef& member) final {
        oldHash = getValueHash(member);
    }

    inline void memberValueChanged(u_int64_t, const AnyValRef& member) final {
        valueRemoved(0, oldHash);
        valueAdded(member);
    }

    inline void iterHasNewValue(const SetValue&,
                                const ValRef<SetValue>& newValue) {
        setValueChanged(*newValue);
    }
};

struct OpSubset::RightSetTrigger : public SetTrigger {
    OpSubset* op;
    u_int64_t oldHash;

    RightSetTrigger(OpSubset* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t hash) final {
        if (getView<SetView>(op->left).getHashIndexMap().count(hash)) {
            op->changeValue([&]() { ++op->violation; return true; });
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        if (getView<SetView>(op->left).getHashIndexMap().count(hash)) {
            op->changeValue([&]() { --op->violation; return true; });
        }
    }

    inline void setValueChanged(const SetView& newValue) final {
        op->changeValue([&]() {
            evaluate(*op, getView<SetView>(op->left), newValue);
            return true;
        });
    }

    inline void possibleMemberValueChange(u_int64_t,
                                          const AnyValRef& member) final {
        oldHash = getValueHash(member);
    }

    inline void memberValueChanged(u_int64_t, const AnyValRef& member) final {
        valueRemoved(0, oldHash);
        valueAdded(member);
    }

    inline void iterHasNewValue(const SetValue&,
                                const ValRef<SetValue>& newValue) {
        setValueChanged(*newValue);
    }
};

OpSubset::OpSubset(OpSubset&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      leftTrigger(std::move(other.leftTrigger)),
      rightTrigger(std::move(other.rightTrigger)) {
    setTriggerParent(this, leftTrigger, rightTrigger);
}

void startTriggering(OpSubset& op) {
    op.leftTrigger = make_shared<LeftSetTrigger>(&op);
    op.rightTrigger = make_shared<RightSetTrigger>(&op);
    addTrigger(op.left, op.leftTrigger);
    addTrigger(op.right, op.rightTrigger);
    startTriggering(op.left);
    startTriggering(op.right);
}

void stopTriggering(OpSubset& op) {
    if (op.leftTrigger) {
        deleteTrigger(op.leftTrigger);
        op.leftTrigger = nullptr;
        stopTriggering(op.left);
    }
    if (op.rightTrigger) {
        deleteTrigger<SetTrigger>(op.rightTrigger);
        op.rightTrigger = nullptr;
        stopTriggering(op.right);
    }
}

void updateViolationDescription(const OpSubset& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.left, op.violation, vioDesc);
    updateViolationDescription(op.right, op.violation, vioDesc);
}

std::shared_ptr<OpSubset> deepCopyForUnroll(const OpSubset& op,
                                            const AnyIterRef& iterator) {
    auto newOpSubset =
        make_shared<OpSubset>(deepCopyForUnroll(op.left, iterator),
                              deepCopyForUnroll(op.right, iterator));
    newOpSubset->violation = op.violation;
    return newOpSubset;
}

std::ostream& dumpState(std::ostream& os, const OpSubset& op) {
    os << "OpSubset: violation=" << op.violation << "\nLeft: ";
    dumpState(os, op.left);
    os << "\nRight: ";
    dumpState(os, op.right);
    return os;
}
