#include "operators/opSubsetEq.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/set.h"
using namespace std;
static const char* NO_UNDEFINED_IN_SUBSET =
    "OpSubSet does not yet handle all the cases where sets become undefined.  "
    "Especially if returned views are undefined\n";

void OpSubsetEq::reevaluateImpl(SetView& leftView, SetView& rightView, bool,
                                bool) {
    violation = 0;
    violatingMembers.clear();
    for (auto& hashIndexPair : leftView.hashIndexMap) {
        if (!rightView.hashIndexMap.count(hashIndexPair.first)) {
            violation += 1;
            violatingMembers.insert(hashIndexPair.second);
        }
    }
}

namespace {
HashType getHashForceDefined(const AnyExprRef& expr) {
    return lib::visit(
        [&](auto& expr) {
            auto view = expr->getViewIfDefined();
            if (!view) {
                myCerr << NO_UNDEFINED_IN_SUBSET;
                myAbort();
            }
            return getValueHash(*view);
        },
        expr);
}

}  // namespace
struct OperatorTrates<OpSubsetEq>::LeftTrigger : public SetTrigger {
   public:
    OpSubsetEq* op;

   public:
    LeftTrigger(OpSubsetEq* op) : op(op) {}
    inline void valueRemovedImpl(UInt memberIndex, HashType hash,
                                 bool trigger) {
        if (!op->right->view()
                 .checkedGet(NO_UNDEFINED_IN_SUBSET)
                 .hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                op->violatingMembers.erase(memberIndex);
                --op->violation;
                return trigger;
            });
        }
    }
    void valueRemoved(UInt index, HashType hash) {
        valueRemovedImpl(index, hash, true);
    }

    inline void valueAddedImpl(UInt memberIndex, HashType hash,
                               bool triggering) {
        if (!op->right->view()
                 .checkedGet(NO_UNDEFINED_IN_SUBSET)
                 .hashIndexMap.count(hash)) {
            op->changeValue([&]() {
                op->violatingMembers.insert(memberIndex);
                ++op->violation;
                return triggering;
            });
        }
    }
    void valueAdded(const AnyExprRef& member) {
        auto hash = getHashForceDefined(member);
        UInt index = op->left->view()
                         .checkedGet(NO_UNDEFINED_IN_SUBSET)
                         .hashIndexMap.at(hash);
        valueAddedImpl(index, hash, true);
    }
    inline void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate(true, false);
            return true;
        });
    }
    void memberReplaced(UInt index, const AnyExprRef& oldMember) {
        memberValueChanged(index, getValueHash(oldMember));
    }
    inline void memberValueChanged(UInt index, HashType oldHash) final {
        HashType newHash = lib::visit(
            [&](auto& members) {
                return getValueHash(
                    members[index]->view().checkedGet(NO_UNDEFINED_IN_SUBSET));
            },
            op->left->view().checkedGet(NO_UNDEFINED_IN_SUBSET).members);
        op->changeValue([&]() {
            valueRemovedImpl(index, oldHash, false);
            valueAddedImpl(index, newHash, false);
            return true;
        });
    }

    inline void memberValuesChanged(
        const std::vector<UInt>& indices,
        const std::vector<HashType>& oldHashes) final {
        op->changeValue([&]() {
            for (size_t i = 0; i < indices.size(); i++) {
                auto index = indices[i];
                auto hash = oldHashes[i];
                valueRemovedImpl(index, hash, false);
            }
            lib::visit(
                [&](auto& members) {
                    for (UInt index : indices) {
                        valueAddedImpl(
                            index,
                            getValueHash(members[index]->view().checkedGet(
                                NO_UNDEFINED_IN_SUBSET)),
                            false);
                    }
                },
                op->left->view().checkedGet(NO_UNDEFINED_IN_SUBSET).members);
            return true;
        });
    }

    void reattachTrigger() final {
        auto trigger = make_shared<OperatorTrates<OpSubsetEq>::LeftTrigger>(op);
        op->left->addTrigger(trigger);
        op->leftTrigger = trigger;
    }
    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
};

struct OperatorTrates<OpSubsetEq>::RightTrigger : public SetTrigger {
    OpSubsetEq* op;
    HashType oldHash;
    vector<HashType> oldHashes;

    RightTrigger(OpSubsetEq* op) : op(op) {}
    inline void valueRemovedImpl(HashType hash, bool triggering) {
        auto& leftView = op->left->view().checkedGet(NO_UNDEFINED_IN_SUBSET);
        auto iter = leftView.hashIndexMap.find(hash);
        if (iter != leftView.hashIndexMap.end()) {
            op->changeValue([&]() {
                op->violatingMembers.insert(iter->second);
                ++op->violation;
                return triggering;
            });
        }
    }
    void valueRemoved(UInt, HashType hash) { valueRemovedImpl(hash, true); }

    inline void valueAddedImpl(HashType hash, bool triggering) {
        auto& leftView = op->left->view().checkedGet(NO_UNDEFINED_IN_SUBSET);
        auto iter = leftView.hashIndexMap.find(hash);
        if (iter != leftView.hashIndexMap.end()) {
            op->changeValue([&]() {
                op->violatingMembers.erase(iter->second);
                --op->violation;
                return triggering;
            });
        }
    }
    void valueAdded(const AnyExprRef& member) {
        valueAddedImpl(getHashForceDefined(member), true);
    }

    inline void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }
    void memberReplaced(UInt index, const AnyExprRef& oldMember) {
        memberValueChanged(index, getValueHash(oldMember));
    }

    inline void memberValueChanged(UInt index, HashType oldHash) final {
        HashType newHash = lib::visit(
            [&](auto& members) {
                return getValueHash(
                    members[index]->view().checkedGet(NO_UNDEFINED_IN_SUBSET));
            },
            op->right->view().checkedGet(NO_UNDEFINED_IN_SUBSET).members);
        op->changeValue([&]() {
            valueRemovedImpl(oldHash, false);
            valueAddedImpl(newHash, false);
            return true;
        });
    }

    inline void memberValuesChanged(
        const std::vector<UInt>& indices,
        const std::vector<HashType>& oldHashes) final {
        op->changeValue([&]() {
            for (HashType hash : oldHashes) {
                valueRemovedImpl(hash, false);
            }
            lib::visit(
                [&](auto& members) {
                    for (UInt index : indices) {
                        valueAddedImpl(
                            getValueHash(members[index]->view().checkedGet(
                                NO_UNDEFINED_IN_SUBSET)),
                            false);
                    }
                },
                op->right->view().checkedGet(NO_UNDEFINED_IN_SUBSET).members);
            return true;
        });
    }
    void reattachTrigger() final {
        auto trigger =
            make_shared<OperatorTrates<OpSubsetEq>::RightTrigger>(op);
        op->right->addTrigger(trigger);
        op->rightTrigger = trigger;
    }
    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
};

void OpSubsetEq::updateVarViolationsImpl(const ViolationContext&,
                                         ViolationContainer& vioContainer) {
    auto leftView = left->getViewIfDefined();
    if (!left->isConstant() && leftView && !violatingMembers.empty()) {
        lib::visit(
            [&](auto& members) {
                for (auto index : violatingMembers) {
                    debug_code(assert(index < members.size()));
                    members[index]->updateVarViolations(violation,
                                                        vioContainer);
                }
            },
            leftView->members);
    } else {
        left->updateVarViolations(violation, vioContainer);
    }
    right->updateVarViolations(violation, vioContainer);
}

void OpSubsetEq::copy(OpSubsetEq&) const {}

std::ostream& OpSubsetEq::dumpState(std::ostream& os) const {
    os << "OpSubsetEq: violation=" << violation << "\nLeft: ";
    left->dumpState(os);
    os << "\nRight: ";
    right->dumpState(os);
    return os;
}

string OpSubsetEq::getOpName() const { return "OpSubsetEq"; }
void OpSubsetEq::debugSanityCheckImpl() const {
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
    UInt checkViolation = 0;

    for (auto& hashIndexPair : leftView.hashIndexMap) {
        if (!rightView.hashIndexMap.count(hashIndexPair.first)) {
            checkViolation += 1;
            sanityCheck(violatingMembers.count(hashIndexPair.second),
                        "Missing index from violating members.");
            ;
        }
    }
    sanityEqualsCheck(checkViolation, violation);
    sanityEqualsCheck(checkViolation, violatingMembers.size());
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSubsetEq> {
    static ExprRef<BoolView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};

ExprRef<BoolView> OpMaker<OpSubsetEq>::make(ExprRef<SetView> l,
                                            ExprRef<SetView> r) {
    return make_shared<OpSubsetEq>(move(l), move(r));
}
