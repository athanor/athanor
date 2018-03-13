#include "operators/opSubsetEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"
#include "base/typeOperations.h"
using namespace std;

using LeftSetTrigger = OpSubsetEq::LeftSetTrigger;
using RightSetTrigger = OpSubsetEq::RightSetTrigger;

inline void evaluate( const SetView& leftSetView,
                     const SetView& rightSetView) {
    violation = 0;
    for (auto& hashIndexPair : leftSetView.hashIndexMap) {
        violation +=
            !rightSetView.hashIndexMap.count(hashIndexPair.first);
    }
}
void evaluate() {
    left->evaluate();
    right->evaluate();
    evaluate(op, getView(left), getView(right));
}

struct OpSubsetEq::LeftSetTrigger : public SetTrigger {
   public:
    OpSubsetEq* op;
    u_int64_t oldHash;

   public:
    LeftSetTrigger(OpSubsetEq* op) : op(op) {}
    inline void valueRemoved(u_int64_t, u_int64_t hash) final {
        if (!op->right->hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                --op->violation;
                return true;
            });
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        if (!op->right->hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
    }

    inline void setValueChanged(const SetView& newValue) final {
        op->changeValue([&]() {
            evaluate(*op, newValue, getView(op->right));
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
        if (op->left->hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
    }

    inline void valueAdded(const AnyValRef& member) final {
        u_int64_t hash = getValueHash(member);
        if (op->left->hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                --op->violation;
                return true;
            });
        }
    }

    inline void setValueChanged(const SetView& newValue) final {
        op->changeValue([&]() {
            evaluate(*op, getView(op->left), newValue);
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

void startTriggering() {
    if (!leftTrigger) {
        leftTrigger = make_shared<LeftSetTrigger>(this);
        rightTrigger = make_shared<RightSetTrigger>(this);
        addTrigger(left, leftTrigger);
        addTrigger(right, rightTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void stopTriggering() {
    if (leftTrigger) {
        deleteTrigger(leftTrigger);
        leftTrigger = nullptr;
        left->stopTriggering();
    }
    if (rightTrigger) {
        deleteTrigger<SetTrigger>(rightTrigger);
        rightTrigger = nullptr;
        right->stopTriggering();
    }
}

void updateViolationDescription( u_int64_t,
                                ViolationDescription& vioDesc) {
    left->updateViolationDescription( violation, vioDesc);
    right->updateViolationDescription( violation, vioDesc);
}

std::shared_ptr<OpSubsetEq> deepCopyForUnroll(
                                              const AnyIterRef& iterator) {
    auto newOpSubsetEq =
        make_shared<OpSubsetEq>(deepCopyForUnroll(left, iterator),
                                deepCopyForUnroll(right, iterator));
    newOpSubsetEq->violation = violation;
    return newOpSubsetEq;
}

std::ostream& dumpState(std::ostream& os, ) {
    os << "OpSubsetEq: violation=" << violation << "\nLeft: ";
    dumpState(os, left);
    os << "\nRight: ";
    dumpState(os, right);
    return os;
}
