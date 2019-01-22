#include "operators/opTogether.h"
#include <iostream>
#include <memory>
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::reevaluate(
    bool recalculateLeftCachedIndex, bool recalculateRightCachedIndex) {
    auto partitionView = partitionOperand->view();
    auto leftView = left->getViewIfDefined();
    auto rightView = right->getViewIfDefined();

    if (!partitionView) {
        this->violation = LARGE_VIOLATION;
        return;
    }
    if (recalculateLeftCachedIndex && leftView) {
        HashType leftHash = getValueHash(*leftView);
        cachedLeftIndex = partitionView->hashIndexMap.at(leftHash);
        reattachPartitionMemberTrigger(true, false);
    }
    if (recalculateRightCachedIndex && rightView) {
        HashType rightHash = getValueHash(*rightView);
        cachedRightIndex = partitionView->hashIndexMap.at(rightHash);
        reattachPartitionMemberTrigger(false, true);
    }
    if (leftView && rightView) {
        this->violation = (partitionView->memberPartMap[cachedLeftIndex] ==
                           partitionView->memberPartMap[cachedRightIndex])
                              ? 0
                              : 1;
    } else {
        violation = LARGE_VIOLATION;
    }
}

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::evaluateImpl() {
    partitionOperand->evaluate();
    left->evaluate();
    right->evaluate();
    reevaluate(true, true);
}

template <typename PartitionMemberViewType>
OpTogether<PartitionMemberViewType>::OpTogether(
    OpTogether<PartitionMemberViewType>&& other)
    : BoolView(move(other)),
      partitionOperand(move(other.partitionOperand)),
      left(move(other.left)),
      right(move(other.right)),
      cachedLeftIndex(move(other.cachedLeftIndex)),
      cachedRightIndex(move(other.cachedRightIndex)),
      partitionOperandTrigger(move(other.partitionOperandTrigger)),
      leftMemberTrigger(move(other.leftMemberTrigger)),
      rightMemberTrigger(move(other.rightMemberTrigger)),
      leftOperandTrigger(move(other.leftOperandTrigger)),
      rightOperandTrigger(move(other.rightOperandTrigger)) {
    setTriggerParent(this, partitionOperandTrigger, leftMemberTrigger,
                     rightMemberTrigger, leftOperandTrigger,
                     rightOperandTrigger);
}

template <typename PartitionMemberViewType>
struct OpTogether<PartitionMemberViewType>::PartitionOperandTrigger
    : public PartitionTrigger {
    OpTogether<PartitionMemberViewType>* op;
    PartitionOperandTrigger(OpTogether<PartitionMemberViewType>* op) : op(op) {}

    void containingPartsSwapped(UInt index1, UInt index2) final {
        ignoreUnused(index1, index2);
        debug_code(assert(
            index1 == op->cachedLeftIndex || index1 == op->cachedRightIndex ||
            index2 == op->cachedLeftIndex || index2 == op->cachedRightIndex));
        op->changeValue([&]() {
            op->reevaluate(false, false);
            return true;
        });
    }

    void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate(true, true);
            return true;
        });
    }

    inline void hasBecomeUndefined() {
        op->changeValue([&]() {
            op->reevaluate(false, false);
            return true;
        });
    }

    void hasBecomeDefined() {
        op->changeValue([&]() {
            op->reevaluate(true, true);
            return true;
        });
    }
    void reattachTrigger() final {
        deleteTrigger(op->partitionOperandTrigger);
        auto trigger = make_shared<PartitionOperandTrigger>(op);
        op->partitionOperand->addTrigger(trigger, false);
        op->reattachPartitionMemberTrigger(true, true);
        op->partitionOperandTrigger = trigger;
    }
};

template <typename PartitionMemberViewType>
template <bool isLeft>
struct OpTogether<PartitionMemberViewType>::OperandTrigger
    : public ChangeTriggerAdapter<
          typename AssociatedTriggerType<PartitionMemberViewType>::type,
          OpTogether<PartitionMemberViewType>::OperandTrigger<isLeft>> {
    typedef typename AssociatedTriggerType<PartitionMemberViewType>::type
        TriggerType;
    OpTogether<PartitionMemberViewType>* op;
    OperandTrigger(OpTogether<PartitionMemberViewType>* op)
        : ChangeTriggerAdapter<TriggerType, OperandTrigger<isLeft>>(
              (isLeft) ? op->left : op->right),
          op(op) {}
    ExprRef<PartitionMemberViewType>& getTriggeringOperand() {
        return (isLeft) ? op->left : op->right;
    }
    void adapterValueChanged() {
        op->changeValue([&]() {
            op->reevaluate(isLeft, !isLeft);
            return true;
        });
    }
    void reattachTrigger() final {
        if (isLeft) {
            reattachLeftTrigger();
        } else {
            reattachRightTrigger();
        }
    }

    void reattachLeftTrigger() {
        deleteTrigger(op->leftOperandTrigger);
        auto trigger = make_shared<
            OpTogether<PartitionMemberViewType>::OperandTrigger<true>>(op);
        op->left->addTrigger(trigger);
        op->leftOperandTrigger = trigger;
    }

    void reattachRightTrigger() {
        deleteTrigger(op->rightOperandTrigger);
        auto trigger = make_shared<
            OpTogether<PartitionMemberViewType>::OperandTrigger<false>>(op);
        op->right->addTrigger(trigger);
        op->rightOperandTrigger = trigger;
    }
    void adapterHasBecomeDefined() {
        op->changeValue([&]() {
            op->reevaluate(isLeft, !isLeft);
            return true;
        });
    }

    void adapterHasBecomeUndefined() {
        op->changeValue([&]() {
            op->reevaluate(false, false);
            return true;
        });
    }
};

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::startTriggeringImpl() {
    if (!partitionOperandTrigger) {
        partitionOperandTrigger = make_shared<PartitionOperandTrigger>(this);
        partitionOperand->addTrigger(partitionOperandTrigger, false);
        reattachPartitionMemberTrigger(true, true);
        partitionOperand->startTriggering();

        leftOperandTrigger = make_shared<OperandTrigger<true>>(this);
        left->addTrigger(leftOperandTrigger);
        left->startTriggering();

        rightOperandTrigger = make_shared<OperandTrigger<false>>(this);
        right->addTrigger(rightOperandTrigger);
        right->startTriggering();
    }
}

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::reattachPartitionMemberTrigger(
    bool left, bool right) {
    if (left) {
        if (leftMemberTrigger) {
            deleteTrigger(leftMemberTrigger);
        }
        leftMemberTrigger = make_shared<PartitionOperandTrigger>(this);
        partitionOperand->addTrigger(leftMemberTrigger, true, cachedLeftIndex);
    }
    if (right) {
        if (rightMemberTrigger) {
            deleteTrigger(rightMemberTrigger);
        }
        rightMemberTrigger = make_shared<PartitionOperandTrigger>(this);
        partitionOperand->addTrigger(rightMemberTrigger, true,
                                     cachedRightIndex);
    }
}

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::stopTriggeringOnChildren() {
    if (partitionOperandTrigger) {
        deleteTrigger(partitionOperandTrigger);
        partitionOperandTrigger = nullptr;
        deleteTrigger(leftOperandTrigger);
        leftOperandTrigger = nullptr;
        deleteTrigger(rightOperandTrigger);
        rightOperandTrigger = nullptr;
        if (leftMemberTrigger) {
            deleteTrigger(leftMemberTrigger);
            leftMemberTrigger = nullptr;
        }
        if (rightMemberTrigger) {
            deleteTrigger(rightMemberTrigger);
            rightMemberTrigger = nullptr;
        }
    }
}

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::stopTriggering() {
    if (partitionOperandTrigger) {
        stopTriggeringOnChildren();
        partitionOperand->stopTriggering();
        left->stopTriggering();
        right->stopTriggering();
    }
}

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::updateVarViolationsImpl(
    const ViolationContext&, ViolationContainer& vioContainer) {
    auto partition = partitionOperand->getViewIfDefined();
    if (!partition) {
        partitionOperand->updateVarViolations(violation, vioContainer);
        return;
    }

    if (!left->appearsDefined()) {
        left->updateVarViolations(violation, vioContainer);
        return;
    }
    if (!right->appearsDefined()) {
        right->updateVarViolations(violation, vioContainer);
        return;
    }
    partition->getMembers<PartitionMemberViewType>()[cachedLeftIndex]
        ->updateVarViolations(violation, vioContainer);
    partition->getMembers<PartitionMemberViewType>()[cachedRightIndex]
        ->updateVarViolations(violation, vioContainer);
}

template <typename PartitionMemberViewType>
ExprRef<BoolView> OpTogether<PartitionMemberViewType>::deepCopyForUnrollImpl(
    const ExprRef<BoolView>&, const AnyIterRef& iterator) const {
    auto newOpTogether = make_shared<OpTogether<PartitionMemberViewType>>(
        partitionOperand->deepCopyForUnroll(partitionOperand, iterator),
        left->deepCopyForUnroll(left, iterator),
        right->deepCopyForUnroll(right, iterator));
    newOpTogether->violation = this->violation;
    newOpTogether->cachedLeftIndex = cachedLeftIndex;
    newOpTogether->cachedRightIndex = cachedRightIndex;
    return newOpTogether;
}

template <typename PartitionMemberViewType>
std::ostream& OpTogether<PartitionMemberViewType>::dumpState(
    std::ostream& os) const {
    os << "opTogether(value=" << this->getViewIfDefined() << ",";
    os << "partition=";
    partitionOperand->dumpState(os) << ",\n";
    os << "left=";
    left->dumpState(os) << ",\n";
    os << "right=";
    right->dumpState(os) << ")";
    return os;
}

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::findAndReplaceSelf(
    const FindAndReplaceFunction& func) {
    this->partitionOperand = findAndReplace(partitionOperand, func);
    this->left = findAndReplace(left, func);
    this->right = findAndReplace(right, func);
}

template <typename PartitionMemberViewType>
pair<bool, ExprRef<BoolView>> OpTogether<PartitionMemberViewType>::optimise(
    PathExtension path) {
    bool changeMade = false;
    auto optResult = partitionOperand->optimise(path.extend(partitionOperand));
    changeMade |= optResult.first;
    partitionOperand = optResult.second;
    auto optResult2 = left->optimise(path.extend(left));
    changeMade |= optResult2.first;
    left = optResult2.second;
    optResult2 = right->optimise(path.extend(right));
    changeMade |= optResult2.first;
    right = optResult2.second;
    return make_pair(changeMade, mpark::get<ExprRef<BoolView>>(path.expr));
}

template <typename PartitionMemberViewType>
string OpTogether<PartitionMemberViewType>::getOpName() const {
    return toString(
        "OpTogether<",
        TypeAsString<
            typename AssociatedValueType<PartitionMemberViewType>::type>::value,
        ">(", left, ",", right, ")");
}

template <typename PartitionMemberViewType>
void OpTogether<PartitionMemberViewType>::debugSanityCheckImpl() const {
    partitionOperand->debugSanityCheck();
    left->debugSanityCheck();
    right->debugSanityCheck();
    auto partitionView = partitionOperand->view();
    auto leftView = left->getViewIfDefined();
    auto rightView = right->getViewIfDefined();
    if (!partitionView || !leftView || !rightView) {
        sanityEqualsCheck(LARGE_VIOLATION, this->violation);
    }
    if (partitionView) {
        if (leftView) {
            HashType leftHash = getValueHash(*leftView);
            sanityEqualsCheck(partitionView->hashIndexMap[leftHash],
                              cachedLeftIndex);
        }
        if (rightView) {
            HashType rightHash = getValueHash(*rightView);
            sanityEqualsCheck(partitionView->hashIndexMap[rightHash],
                              cachedRightIndex);
        }
        if (leftView && rightView) {
            bool equalParts = partitionView->memberPartMap[cachedLeftIndex] ==
                              partitionView->memberPartMap[cachedRightIndex];
            if (equalParts) {
                sanityEqualsCheck(0, this->violation);
            } else {
                sanityEqualsCheck(1, this->violation);
            }
        }
    }
}

template <typename Op>
struct OpMaker;

template <typename View>
struct OpMaker<OpTogether<View>> {
    static ExprRef<BoolView> make(ExprRef<PartitionView> partition,
                                  ExprRef<View> left, ExprRef<View> right);
};

template <typename View>
ExprRef<BoolView> OpMaker<OpTogether<View>>::make(
    ExprRef<PartitionView> partition, ExprRef<View> left, ExprRef<View> right) {
    return make_shared<OpTogether<View>>(move(partition), move(left),
                                         move(right));
}

#define opTogetherInstantiators(name)       \
    template struct OpTogether<name##View>; \
    template struct OpMaker<OpTogether<name##View>>;

buildForAllTypes(opTogetherInstantiators, );
#undef opTogetherInstantiators
