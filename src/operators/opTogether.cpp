#include "operators/opTogether.h"
#include "operators/simpleOperator.hpp"

using namespace std;

OpTogether::~OpTogether() {
    deleteTrigger(this->leftTrigger.getTrigger());
    deleteTrigger(this->rightTrigger.getTrigger());
    for (auto& trigger : partitionMemberTriggers) {
        deleteTrigger(trigger.getTrigger());
    }
}
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
        deleteTrigger(op->partitionMemberTriggers[indexOfRemovedValue].getTrigger());
        op->partitionMemberTriggers[indexOfRemovedValue] =
            move(op->partitionMemberTriggers.back());
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
        deleteTrigger(op->partitionMemberTriggers[index].getTrigger());
        op->partitionMemberTriggers[index] = nullptr;
        if (partitionView.hashIndexMap.count(hash)) {
            deleteTrigger(op->partitionMemberTriggers[index].getTrigger());
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
            deleteTrigger(op->partitionMemberTriggers[index].getTrigger());
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
    for (auto& trigger: op.partitionMemberTriggers) {
        deleteTrigger(trigger.getTrigger());
    }
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
        left->startTriggering();
        right->startTriggering();
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
void updateVarViolationsOnSet(SetView& leftView, PartitionView& rightView,
                              UInt violation,
                              ViolationContainer& vioContainer) {
    mpark::visit(
        [&](auto& members) {
            typedef viewType(members) View;
            auto& partitionMembers = rightView.getMembers<View>();
            for (size_t i = 0; i < members.size(); i++) {
                auto& member = members[i];
                auto& hash = leftView.indexHashMap[i];
                member->updateVarViolations(violation, vioContainer);
                if (rightView.hashIndexMap.count(hash)) {
                    partitionMembers[rightView.hashIndexMap[hash]]
                        ->updateVarViolations(violation, vioContainer);
                }
            }
        },
        leftView.members);
}

void OpTogether::updateVarViolationsImpl(const ViolationContext& vioContext,
                                         ViolationContainer& vioContainer) {
    auto* boolVioContextTest =
        dynamic_cast<const BoolViolationContext*>(&vioContext);
    UInt violationToReport = (boolVioContextTest && boolVioContextTest->negated)
                                 ? boolVioContextTest->parentViolation
                                 : this->violation;

    if (violationToReport == 0) {
        return;
    } else if (allOperandsAreDefined()) {
        auto& leftView =
            left->getViewIfDefined().checkedGet("should be defined here.");
        auto& rightView =
            right->getViewIfDefined().checkedGet("should be defined here.");
        updateVarViolationsOnSet(leftView, rightView, violationToReport,
                                 vioContainer);
    } else {
        left->updateVarViolations(violationToReport, vioContainer);
        right->updateVarViolations(violationToReport, vioContainer);
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
