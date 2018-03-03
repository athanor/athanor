#include "operators/opSubsetEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"
#include "types/typeOperations.h"
using namespace std;

using LeftSetTrigger = OpSubsetEq::LeftSetTrigger;
using RightSetTrigger = OpSubsetEq::RightSetTrigger;

inline void evaluate(OpSubsetEq& op, const SetView& leftSetView,
                     const SetView& rightSetView) {
    op.violation = 0;
    for (auto& hashIndexPair : leftSetView.hashIndexMap) {
        op.violation +=
            !rightSetView.hashIndexMap.count(hashIndexPair.first);
    }
}
void evaluate(OpSubsetEq& op) {
    evaluate(op.left);
    evaluate(op.right);
    evaluate(op, getView<SetView>(op.left), getView<SetView>(op.right));
}

struct OpSubsetEq::LeftSetTrigger : public SetTrigger {
   public:
    OpSubsetEq* op;
    u_int64_t oldHash;

   public:
    LeftSetTrigger(OpSubsetEq* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t hash) final {
        if (!getView<SetView>(op->right).hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                --op->violation;
                return true;
            });
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        if (!getView<SetView>(op->right).hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
    }

    inline void setValueChanged(const SetView& newValue) final {
        op->changeValue([&]() {
            evaluate(*op, newValue, getView<SetView>(op->right));
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

struct OpSubsetEq::RightSetTrigger : public SetTrigger {
    OpSubsetEq* op;
    u_int64_t oldHash;

    RightSetTrigger(OpSubsetEq* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t hash) final {
        if (getView<SetView>(op->left).hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        if (getView<SetView>(op->left).hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                --op->violation;
                return true;
            });
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

OpSubsetEq::OpSubsetEq(OpSubsetEq&& other)
    : BoolView(std::move(other)),
      left(std::move(other.left)),
      right(std::move(other.right)),
      leftTrigger(std::move(other.leftTrigger)),
      rightTrigger(std::move(other.rightTrigger)) {
    setTriggerParent(this, leftTrigger, rightTrigger);
}

void startTriggering(OpSubsetEq& op) {
    if (!op.leftTrigger) {
        op.leftTrigger = make_shared<LeftSetTrigger>(&op);
        op.rightTrigger = make_shared<RightSetTrigger>(&op);
        addTrigger(op.left, op.leftTrigger);
        addTrigger(op.right, op.rightTrigger);
        startTriggering(op.left);
        startTriggering(op.right);
    }
}

void stopTriggering(OpSubsetEq& op) {
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

void updateViolationDescription(const OpSubsetEq& op, u_int64_t,
                                ViolationDescription& vioDesc) {
    updateViolationDescription(op.left, op.violation, vioDesc);
    updateViolationDescription(op.right, op.violation, vioDesc);
}

std::shared_ptr<OpSubsetEq> deepCopyForUnroll(const OpSubsetEq& op,
                                              const AnyIterRef& iterator) {
    auto newOpSubsetEq =
        make_shared<OpSubsetEq>(deepCopyForUnroll(op.left, iterator),
                                deepCopyForUnroll(op.right, iterator));
    newOpSubsetEq->violation = op.violation;
    return newOpSubsetEq;
}

std::ostream& dumpState(std::ostream& os, const OpSubsetEq& op) {
    os << "OpSubsetEq: violation=" << op.violation << "\nLeft: ";
    dumpState(os, op.left);
    os << "\nRight: ";
    dumpState(os, op.right);
    return os;
}
