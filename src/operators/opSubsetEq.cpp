#include "operators/opSubsetEq.h"
#include <iostream>
#include <memory>
#include "types/set.h"
using namespace std;

void OpSubsetEq::reevaluate() {
    violation = 0;
    for (auto& hashIndexPair : left->view().hashIndexMap) {
        violation += !right->view().hashIndexMap.count(hashIndexPair.first);
    }
}

struct OperatorTrates<OpSubsetEq>::LeftTrigger : public SetTrigger {
   public:
    OpSubsetEq* op;
    HashType oldHash;

   public:
    LeftTrigger(OpSubsetEq* op) : op(op) {}
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
            op->reevaluate();
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
    void reattachTrigger() final {
        deleteTrigger(op->leftTrigger);
        auto trigger = make_shared<OperatorTrates<OpSubsetEq>::LeftTrigger>(op);
        op->left->addTrigger(trigger);
        op->leftTrigger = trigger;
    }
};

struct OperatorTrates<OpSubsetEq>::RightTrigger : public SetTrigger {
    OpSubsetEq* op;
    HashType oldHash;

    RightTrigger(OpSubsetEq* op) : op(op) {}
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
            op->reevaluate();
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
    void reattachTrigger() final {
        deleteTrigger(op->rightTrigger);
        auto trigger =
            make_shared<OperatorTrates<OpSubsetEq>::RightTrigger>(op);
        op->left->addTrigger(trigger);
        op->rightTrigger = trigger;
    }
};

void OpSubsetEq::updateViolationDescription(UInt,
                                            ViolationDescription& vioDesc) {
    left->updateViolationDescription(violation, vioDesc);
    right->updateViolationDescription(violation, vioDesc);
}

void OpSubsetEq::copy(OpSubsetEq& newOp) const { newOp.violation = violation; }

std::ostream& OpSubsetEq::dumpState(std::ostream& os) const {
    os << "OpSubsetEq: violation=" << violation << "\nLeft: ";
    left->dumpState(os);
    os << "\nRight: ";
    right->dumpState(os);
    return os;
}
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSubsetEq> {
    ExprRef<BoolView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};

ExprRef<BoolView> OpMaker<OpSubsetEq>::make(ExprRef<SetView> l,
                                            ExprRef<SetView> r) {
    return make_shared<OpSubsetEq>(move(l), move(r));
}
