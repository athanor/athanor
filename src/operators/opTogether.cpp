#include "operators/opTogether.h"
#include "operators/simpleOperator.hpp"

using namespace std;

static void deletePartitionTriggers(OpTogether& op);
static void addTrigger(OpTogether& op, const HashType& hash,
                       PartitionView& view);
static void reattachAllTriggers(OpTogether& op);

void OpTogether::reevaluateImpl(SetView& leftView, PartitionView& rightView,
                                bool, bool) {
    if (leftView.numberElements() == 0) {
        violation = 0;
        return;
    }
    HashMap<UInt, UInt> partCounts;
    for (auto& hash : leftView.indexHashMap) {
        if (rightView.hashIndexMap.count(hash)) {
            UInt part =
                rightView.memberPartMap[rightView.hashIndexMap.at(hash)];
            partCounts[part] += 1;
        } else {
            this->setDefined(false);
            return;
        }
    }
    bool first = true;
    for (auto& hashCountPair : partCounts) {
        auto diff = leftView.numberElements() - hashCountPair.second;
        if (first || diff < violation) {
            first = false;
            violation = diff;
        }
    }
}

struct OperatorTrates<OpTogether>::RightTrigger : public PartitionTrigger {
    OpTogether* op;
    RightTrigger(OpTogether* op) : op(op) {}
    void valueChanged() final {
        reattachAllTriggers(*op);
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }

    void hasBecomeUndefined() final {
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }

    void hasBecomeDefined() final {
        reattachAllTriggers(*op);
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }
    void memberReplaced(UInt, const AnyExprRef&) final {
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }

    void containingPartsSwapped(UInt, UInt) final {
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }
    void membersMovedToPart(const std::vector<UInt>&, const std::vector<UInt>&,
                            UInt) final {
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }
    void membersMovedFromPart(UInt, const std::vector<UInt>&,
                              const std::vector<UInt>&) final {
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }

    void reattachTrigger() final { reattachAllTriggers(*op); }
};

struct OperatorTrates<OpTogether>::LeftTrigger : public SetTrigger {
    OpTogether* op;
    LeftTrigger(OpTogether* op) : op(op) {}
    void valueRemoved(UInt indexOfRemovedValue, HashType) final {
        op->partitionMemberTriggers[indexOfRemovedValue] =
            op->partitionMemberTriggers.back();
        op->partitionMemberTriggers.pop_back();
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }
    void valueAdded(const AnyExprRef& member) {
        auto viewOption = op->right->getViewIfDefined();
        if (!viewOption) {
            return;
        }
        auto& view = *viewOption;
        HashType hash = getValueHash(member);
        addTrigger(*op, hash, view);
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }
    void handleMemberChange(UInt index) {
        auto setViewOption = op->left->getViewIfDefined();
        auto partitionViewOption = op->right->getViewIfDefined();
        if (!setViewOption || !partitionViewOption) {
            return;
        }
        auto& setView = *setViewOption;
        auto& partitionView = *partitionViewOption;
        HashType hash = setView.indexHashMap[index];
        op->partitionMemberTriggers[index] = nullptr;
        if (partitionView.hashIndexMap.count(hash)) {
            op->partitionMemberTriggers[index] =
                make_shared<OperatorTrates<OpTogether>::RightTrigger>(op);
            op->right->addTrigger(op->partitionMemberTriggers[index], true,
                                  partitionView.hashIndexMap.at(hash));
        }
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }

    void memberValueChanged(UInt index, HashType) final {
        handleMemberChange(index);
    }

    void memberReplaced(UInt index, const AnyExprRef&) final {
        handleMemberChange(index);
    }
    void memberValuesChanged(const std::vector<UInt>& indices,
                             const std::vector<HashType>&) final {
        auto setViewOption = op->left->getViewIfDefined();
        auto partitionViewOption = op->right->getViewIfDefined();
        if (!setViewOption || !partitionViewOption) {
            return;
        }
        auto& setView = *setViewOption;
        auto& partitionView = *partitionViewOption;
        for (auto index : indices) {
            HashType hash = setView.indexHashMap[index];
            op->partitionMemberTriggers[index] = nullptr;
            if (partitionView.hashIndexMap.count(hash)) {
                op->partitionMemberTriggers[index] =
                    make_shared<OperatorTrates<OpTogether>::RightTrigger>(op);
                op->right->addTrigger(op->partitionMemberTriggers[index], true,
                                      partitionView.hashIndexMap.at(hash));
            }
        }
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }

    void valueChanged() final {
        reattachAllTriggers(*op);
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }
    void reattachTrigger() final { reattachAllTriggers(*op); }
    void hasBecomeUndefined() final {
        deletePartitionTriggers(*op);
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }
    void hasBecomeDefined() {
        reattachAllTriggers(*op);
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }
};

static void deletePartitionTriggers(OpTogether& op) {
    op.rightTrigger = nullptr;
    op.partitionMemberTriggers.clear();
}

static void addTrigger(OpTogether& op, const HashType& hash,
                       PartitionView& view) {
    shared_ptr<OperatorTrates<OpTogether>::RightTrigger> trigger = nullptr;
    if (view.hashIndexMap.count(hash)) {
        trigger = make_shared<OperatorTrates<OpTogether>::RightTrigger>(&op);
        op.right->addTrigger(trigger, true, view.hashIndexMap.at(hash));
    }
    op.partitionMemberTriggers.emplace_back(move(trigger));
}

static void reattachAllTriggers(OpTogether& op) {
    op.leftTrigger = nullptr;
    deletePartitionTriggers(op);
    auto setViewOption = op.left->getViewIfDefined();
    auto partitionViewOption = op.right->getViewIfDefined();
    if (!setViewOption) {
        return;
    }
    auto& setView = *setViewOption;
    op.leftTrigger = make_shared<OperatorTrates<OpTogether>::LeftTrigger>(&op);
    op.left->addTrigger(op.leftTrigger);

    if (!partitionViewOption) {
        return;
    }
    auto& partitionView = *partitionViewOption;
    op.rightTrigger =
        make_shared<OperatorTrates<OpTogether>::RightTrigger>(&op);
    op.right->addTrigger(op.rightTrigger, false, -1);
    for (auto hash : setView.indexHashMap) {
        addTrigger(op, hash, partitionView);
    }
}

void OpTogether::startTriggeringImpl() {
    if (!leftTrigger) {
        reattachAllTriggers(*this);
    }
}
void OpTogether::stopTriggering() {
    if (leftTrigger) {
        this->leftTrigger = nullptr;
        deletePartitionTriggers(*this);
        this->left->stopTriggering();
        this->right->stopTriggering();
    }
}
void updateVarViolationsOnSet(ExprRef<SetView>& left, UInt violation,
                              ViolationContainer& vioContainer) {
    auto viewOption = left->getViewIfDefined();
    if (!viewOption) {
        left->updateVarViolations(violation, vioContainer);
        return;
    }
    auto& view = *viewOption;
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member->updateVarViolations(violation, vioContainer);
            }
        },
        view.members);
}

void OpTogether::updateVarViolationsImpl(const ViolationContext&,
                                         ViolationContainer& vioContainer) {
    if (violation == 0) {
        return;
    } else if (allOperandsAreDefined()) {
        updateVarViolationsOnSet(this->left, violation, vioContainer);
        right->updateVarViolations(violation, vioContainer);
    } else {
        left->updateVarViolations(violation, vioContainer);
        right->updateVarViolations(violation, vioContainer);
    }
}

void OpTogether::copy(OpTogether&) const {}

ostream& OpTogether::dumpState(ostream& os) const {
    os << "OpTogether: violation=" << violation << "\nleft: ";
    left->dumpState(os);
    os << "\nright: ";
    right->dumpState(os);
    return os;
}

string OpTogether::getOpName() const { return "OpTogether"; }
void OpTogether::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    auto leftOption = left->getViewIfDefined();
    auto rightOption = right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        sanityLargeViolationCheck(violation);
        return;
    }
    auto& leftView = *leftOption;
    auto& rightView = *rightOption;
    if (leftView.numberElements() == 0) {
        sanityEqualsCheck(0, violation);
        return;
    }
    HashMap<UInt, UInt> partCounts;
    for (auto& hash : leftView.indexHashMap) {
        if (rightView.hashIndexMap.count(hash)) {
            UInt part =
                rightView.memberPartMap[rightView.hashIndexMap.at(hash)];
            partCounts[part] += 1;
        } else {
            sanityLargeViolationCheck(violation);
            return;
        }
    }
    UInt checkViolation;
    bool first = true;
    for (auto& hashCountPair : partCounts) {
        auto diff = leftView.numberElements() - hashCountPair.second;
        if (first || diff < checkViolation) {
            first = false;
            checkViolation = diff;
        }
    }
    sanityEqualsCheck(checkViolation, violation);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpTogether> {
    static ExprRef<BoolView> make(ExprRef<SetView> l, ExprRef<PartitionView> r);
};

ExprRef<BoolView> OpMaker<OpTogether>::make(ExprRef<SetView> l,
                                            ExprRef<PartitionView> r) {
    return make_shared<OpTogether>(move(l), move(r));
}
