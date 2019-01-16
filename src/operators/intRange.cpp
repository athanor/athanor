#include "operators/intRange.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/intVal.h"

using namespace std;

void IntRange::reevaluateImpl(IntView& leftView, IntView& rightView) {
    cachedLower = leftView.value;
    cachedUpper = rightView.value;
    this->silentClear();
    for (Int i = cachedLower; i <= cachedUpper; i++) {
        auto val = make<IntValue>();
        val->setConstant(true);
        val->value = i;
        this->addMember(this->numberElements(), val.asExpr());
    }
}
template <bool isLeft>
struct OperatorTrates<IntRange>::Trigger : public IntTrigger {
    IntRange* op;
    Trigger(IntRange* op) : op(op) {}
    void valueChanged() final {
        if (!op->isDefined()) {
            return;
        }
        if (isLeft) {
            auto leftView = op->left->getViewIfDefined();
            if (!leftView) {
                hasBecomeUndefined();
                return;
            }
            adjustLower(*leftView, true);
        } else {
            auto rightView = op->right->getViewIfDefined();
            if (!rightView) {
                hasBecomeUndefined();
                return;
            }
            adjustUpper(*rightView, true);
        }
    }

    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() {
        auto leftView = op->left->getViewIfDefined();
        if (!leftView) {
            return;
        }
        auto rightView = op->right->getViewIfDefined();
        if (!rightView) {
            return;
        }
        op->setDefined(true);
        if (isLeft) {
            adjustLower(*leftView, false);
        } else {
            adjustUpper(*rightView, false);
        }
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers);
    }
    inline void adjustLower(const IntView& leftView, bool trigger) {
        Int newLower = leftView.value;
        while (op->cachedLower > newLower) {
            --op->cachedLower;
            if (op->cachedLower > op->cachedUpper) {
                continue;
            }
            auto val = make<IntValue>();
            val->value = op->cachedLower;
            if (trigger) {
                op->addMemberAndNotify(0, val.asExpr());
            } else {
                op->addMember(0, val.asExpr());
            }
        }
        while (op->cachedLower < newLower) {
            ++op->cachedLower;
            if (op->cachedLower > op->cachedUpper + 1) {
                continue;
            }
            if (trigger) {
                op->removeMemberAndNotify<IntView>(0);
            } else {
                op->removeMember<IntView>(0);
            }
        }
    }
    inline void adjustUpper(const IntView& rightView, bool trigger) {
        Int newUpper = rightView.value;
        while (op->cachedUpper < newUpper) {
            ++op->cachedUpper;
            if (op->cachedUpper < op->cachedLower) {
                continue;
            }
            auto val = make<IntValue>();
            val->value = op->cachedUpper;
            if (trigger) {
                op->addMemberAndNotify(op->numberElements(), val.asExpr());
            } else {
                op->addMember(op->numberElements(), val.asExpr());
            }
        }
        while (op->cachedUpper > newUpper) {
            --op->cachedUpper;
            if (op->cachedUpper < op->cachedLower - 1) {
                continue;
            }
            if (trigger) {
                op->removeMemberAndNotify<IntView>(op->numberElements() - 1);
            } else {
                op->removeMember<IntView>(op->numberElements() - 1);
            }
        }
    }

    inline void reattachTrigger() final {
        if (isLeft) {
            reassignLeftTrigger();
        } else {
            reassignRightTrigger();
        }
    }
    inline void reassignLeftTrigger() {
        deleteTrigger(op->leftTrigger);
        auto newTrigger = make_shared<Trigger<true>>(op);
        op->left->addTrigger(newTrigger);
        op->leftTrigger = newTrigger;
    }
    inline void reassignRightTrigger() {
        deleteTrigger(op->rightTrigger);
        auto newTrigger = make_shared<Trigger<false>>(op);
        op->right->addTrigger(newTrigger);
        op->rightTrigger = newTrigger;
    }
};

void IntRange::updateVarViolationsImpl(const ViolationContext& vioContext,
                                       ViolationContainer& vioContainer) {
    left->updateVarViolations(vioContext, vioContainer);
    right->updateVarViolations(vioContext, vioContainer);
}

void IntRange::copy(IntRange& newOp) const {
    newOp.members = this->members;
    newOp.cachedLower = cachedLower;
    newOp.cachedUpper = cachedUpper;
}

ostream& IntRange::dumpState(ostream& os) const {
    os << "IntRange(\nleft: ";
    left->dumpState(os);
    os << ",\nright: ";
    right->dumpState(os);
    os << ")";
    return os;
    return os;
}

template <typename Op>
struct OpMaker;
template <>
struct OpMaker<IntRange> {
    static ExprRef<SequenceView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

ExprRef<SequenceView> OpMaker<IntRange>::make(ExprRef<IntView> l,
                                              ExprRef<IntView> r) {
    return make_shared<IntRange>(move(l), move(r));
}

string IntRange::getOpName() const {
    return toString("IntRange(", left->getViewIfDefined(), ",",
                    right->getViewIfDefined(), ")");
}

void IntRange::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    if (!this->appearsDefined()) {
        return;
    }
    auto& leftView = left->view().get();
    auto& rightView = right->view().get();
    sanityEqualsCheck(0, numberUndefined);
    Int value = max(rightView.value - (leftView.value - 1), (Int)0);
    sanityEqualsCheck(value, (Int)numberElements());

    size_t index = 0;
    for (Int i = leftView.value; i <= rightView.value; i++) {
        auto memberView = getMembers<IntView>()[index]->getViewIfDefined();
        sanityCheck(memberView, "One of the sequence members is undefined.");
        sanityCheck(memberView->value == i,
                    toString("One member has value ", memberView->value,
                             " but should be ", i, "."));
        ++index;
    }
}
