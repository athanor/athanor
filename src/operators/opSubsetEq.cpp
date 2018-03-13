#include "operators/opSubsetEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"
using namespace std;

using LeftSetTrigger = OpSubsetEq::LeftSetTrigger;
using RightSetTrigger = OpSubsetEq::RightSetTrigger;

inline void evaluate(OpSubsetEq& op, const SetView& left,
                     const SetView& right) {
    op.violation = 0;
    for (auto& hashIndexPair : left.hashIndexMap) {
        op.violation += right.hashIndexMap.count(hashIndexPair.first);
    }
}

void OpSubsetEq::evaluate() {
    left->evaluate();
    right->evaluate();
    ::evaluate(*this, *left, *right);
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

    inline void valueAdded(const AnyExprRef& member) final {
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
            ::evaluate(*op, newValue, *(op->right));
            return true;
        });
    }

    inline void possibleMemberValueChange(u_int64_t,
                                          const AnyExprRef& member) final {
        oldHash = getValueHash(member);
    }

    inline void memberValueChanged(u_int64_t, const AnyExprRef& member) final {
        valueRemoved(0, oldHash);
        valueAdded(member);
    }

    inline void iterHasNewValue(const SetView&,
                                const ExprRef<SetView>& newValue) {
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

    inline void valueAdded(const AnyExprRef& member) final {
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
            ::evaluate(*op, *(op->left), newValue);
            return true;
        });
    }

    inline void possibleMemberValueChange(u_int64_t,
                                          const AnyExprRef& member) final {
        oldHash = getValueHash(member);
    }

    inline void memberValueChanged(u_int64_t, const AnyExprRef& member) final {
        valueRemoved(0, oldHash);
        valueAdded(member);
    }

    inline void iterHasNewValue(const SetView&,
                                const ExprRef<SetView>& newValue) {
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

void OpSubsetEq::startTriggering() {
    if (!leftTrigger) {
        leftTrigger = make_shared<LeftSetTrigger>(this);
        rightTrigger = make_shared<RightSetTrigger>(this);
        addTrigger(left, leftTrigger);
        addTrigger(right, rightTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpSubsetEq::stopTriggering() {
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

void OpSubsetEq::updateViolationDescription(u_int64_t,
                                            ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

ExprRef<BoolView> OpSubsetEq::deepCopySelfForUnroll(
    const ExprRef<BoolView>&, const AnyIterRef& iterator) const {
    auto newOpSubsetEq =
        make_shared<OpSubsetEq>(left->deepCopySelfForUnroll(left, iterator),
                                right->deepCopySelfForUnroll(right, iterator));
    newOpSubsetEq->violation = violation;
    return ViewRef<BoolView>(newOpSubsetEq);
}

std::ostream& OpSubsetEq::dumpState(std::ostream& os) const {
    os << "OpSubsetEq: violation=" << violation << "\nLeft: ";
    left->dumpState(os);
    os << "\nRight: ";
    right->dumpState(os);
    return os;
}
