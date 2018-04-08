#include "operators/opSubsetEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"
using namespace std;

using LeftSetTrigger = OpSubsetEq::LeftSetTrigger;
using RightSetTrigger = OpSubsetEq::RightSetTrigger;

inline void evaluate(OpSubsetEq& op) {
    op.violation = 0;
    for (auto& hashIndexPair : op.left->view().hashIndexMap) {
        op.violation +=
            !op.right->view().hashIndexMap.count(hashIndexPair.first);
    }
}

void OpSubsetEq::evaluate() {
    left->evaluate();
    right->evaluate();
    ::evaluate(*this);
}

struct OpSubsetEq::LeftSetTrigger : public SetTrigger {
   public:
    OpSubsetEq* op;
    HashType oldHash;

   public:
    LeftSetTrigger(OpSubsetEq* op) : op(op) {}
    inline void valueRemoved(UInt, HashType hash) final {
        if (!op->right->view().hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                --op->violation;
                return true;
            });
        }
    }

    inline void valueAdded(const AnyExprRef& member) final {
        HashType hash = getValueHash(member);
        if (!op->right->view().hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
    }
    void possibleValueChange() final {}
    inline void valueChanged() final {
        op->changeValue([&]() {
            ::evaluate(*op);
            return true;
        });
    }

    inline void possibleMemberValueChange(UInt,
                                          const AnyExprRef& member) final {
        oldHash = getValueHash(member);
    }

    inline void memberValueChanged(UInt, const AnyExprRef& member) final {
        valueRemoved(0, oldHash);
        valueAdded(member);
    }
};

struct OpSubsetEq::RightSetTrigger : public SetTrigger {
    OpSubsetEq* op;
    HashType oldHash;

    RightSetTrigger(OpSubsetEq* op) : op(op) {}
    inline void valueRemoved(UInt, HashType hash) final {
        if (op->left->view().hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
    }

    inline void valueAdded(const AnyExprRef& member) final {
        HashType hash = getValueHash(member);
        if (op->left->view().hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                --op->violation;
                return true;
            });
        }
    }
    void possibleValueChange() final {}
    inline void valueChanged() final {
        op->changeValue([&]() {
            ::evaluate(*op);
            return true;
        });
    }

    inline void possibleMemberValueChange(UInt,
                                          const AnyExprRef& member) final {
        oldHash = getValueHash(member);
    }

    inline void memberValueChanged(UInt, const AnyExprRef& member) final {
        valueRemoved(0, oldHash);
        valueAdded(member);
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
        left->addTrigger(leftTrigger);
        right->addTrigger(rightTrigger);
        left->startTriggering();
        right->startTriggering();
    }
}

void OpSubsetEq::stopTriggeringOnChildren() {
    if (leftTrigger) {
        deleteTrigger(leftTrigger);
        leftTrigger = nullptr;
    }
    if (rightTrigger) {
        deleteTrigger<SetTrigger>(rightTrigger);
        rightTrigger = nullptr;
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

void OpSubsetEq::updateViolationDescription(UInt,
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
    return newOpSubsetEq;
}

std::ostream& OpSubsetEq::dumpState(std::ostream& os) const {
    os << "OpSubsetEq: violation=" << violation << "\nLeft: ";
    left->dumpState(os);
    os << "\nRight: ";
    right->dumpState(os);
    return os;
}

void OpSubsetEq::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    this->left = findAndReplace(left, func);
    this->right = findAndReplace(right, func);
}
