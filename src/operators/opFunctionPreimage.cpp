#include "operators/opFunctionPreimage.h"

#include <iostream>
#include <memory>

#include "operators/simpleOperator.hpp"


using namespace std;
template<typename OperandView>
void OpFunctionPreimage::reevaluateImpl(OperandView& image, FunctionView& function,
                                    bool, bool) {
silentClear();
										
}
namespace {
HashType getHashForceDefined(const AnyExprRef& expr) {
    return lib::visit(
        [&](auto& expr) {
            auto view = expr->getViewIfDefined();
            if (!view) {
                myCerr << NO_UNDEFINED_IN_SUBMSET;
                myAbort();
            }
            return getValueHash(*view);
        },
        expr);
}

}  // namespace

struct OperatorTrates<OpFunctionPreimage>::LeftTrigger : public MSetTrigger {
   public:
    OpFunctionPreimage* op;

   public:
    LeftTrigger(OpFunctionPreimage* op) : op(op) {}
    void valueRemoved(UInt, const AnyExprRef& member) {
        op->changeValue([&]() {
            updateViolation(op, getHashForceDefined(member));
            return true;
        });
    }

    void valueAdded(const AnyExprRef& member) final {
        op->changeValue([&]() {
            updateViolation(op, getHashForceDefined(member));
            return true;
        });
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
        auto& leftView =
            op->left->getViewIfDefined().checkedGet(NO_UNDEFINED_IN_SUBMSET);
        HashType newHash = leftView.indexHashMap[index];
        op->changeValue([&]() {
            updateViolation(op, oldHash);
            updateViolation(op, newHash);
            return true;
        });
    }
    inline void memberValuesChanged(
        const std::vector<UInt>& indices,
        const std::vector<HashType>& oldHashes) final {
        op->changeValue([&]() {
            for (HashType hash : oldHashes) {
                updateViolation(op, hash);
            }
            auto& indexHashMap = op->left->view()
                                     .checkedGet(NO_UNDEFINED_IN_SUBMSET)
                                     .indexHashMap;
            for (UInt index : indices) {
                updateViolation(op, indexHashMap[index]);
            }
            return true;
        });
    }

    void reattachTrigger() final {
        deleteTrigger(op->leftTrigger);
        auto trigger =
            make_shared<OperatorTrates<OpFunctionPreimage>::LeftTrigger>(op);
        op->left->addTrigger(trigger);
        op->leftTrigger = trigger;
    }
    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
};

struct OperatorTrates<OpFunctionPreimage>::RightTrigger : public MSetTrigger {
    OpFunctionPreimage* op;
    RightTrigger(OpFunctionPreimage* op) : op(op) {}
    void valueRemoved(UInt, const AnyExprRef& member) {
        op->changeValue([&]() {
            updateViolation(op, getHashForceDefined(member));
            return true;
        });
    }

    void valueAdded(const AnyExprRef& member) final {
        op->changeValue([&]() {
            updateViolation(op, getHashForceDefined(member));
            return true;
        });
    }

    inline void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate(false, true);
            return true;
        });
    }
    void memberReplaced(UInt index, const AnyExprRef& oldMember) {
        memberValueChanged(index, getHashForceDefined(oldMember));
    }

    inline void memberValueChanged(UInt index, HashType oldHash) final {
        auto& rightView = op->right->view().checkedGet(NO_UNDEFINED_IN_SUBMSET);
        HashType newHash = rightView.indexHashMap[index];
        op->changeValue([&]() {
            updateViolation(op, oldHash);
            updateViolation(op, newHash);
            return true;
        });
    }

    inline void memberValuesChanged(
        const std::vector<UInt>& indices,
        const std::vector<HashType>& oldHashes) final {
        op->changeValue([&]() {
            for (HashType hash : oldHashes) {
                updateViolation(op, hash);
            }
            auto& indexHashMap = op->right->view()
                                     .checkedGet(NO_UNDEFINED_IN_SUBMSET)
                                     .indexHashMap;
            for (UInt index : indices) {
                updateViolation(op, indexHashMap[index]);
            }
            return true;
        });
    }

    void reattachTrigger() final {
        deleteTrigger(op->rightTrigger);
        auto trigger =
            make_shared<OperatorTrates<OpFunctionPreimage>::RightTrigger>(op);
        op->right->addTrigger(trigger);
        op->rightTrigger = trigger;
    }
    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
};

void OpFunctionPreimage::updateVarViolationsImpl(const ViolationContext&,
                                             ViolationContainer& vioContainer) {
    left->updateVarViolations(violation, vioContainer);
    right->updateVarViolations(violation, vioContainer);
}

void OpFunctionPreimage::copy(OpFunctionPreimage&) const {}

std::ostream& OpFunctionPreimage::dumpState(std::ostream& os) const {
    os << "OpFunctionPreimage: violation=" << violation << "\nLeft: ";
    left->dumpState(os);
    os << "\nRight: ";
    right->dumpState(os);
    return os;
}

string OpFunctionPreimage::getOpName() const { return "OpFunctionPreimage"; }
void OpFunctionPreimage::debugSanityCheckImpl() const {
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
    for (auto& hashCountPair : leftView.memberCounts) {
        auto rightCount = rightView.memberCount(hashCountPair.first);
        if (hashCountPair.second > rightCount) {
            checkViolation += hashCountPair.second - rightCount;
        }
    }
    sanityEqualsCheck(checkViolation, violation);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpFunctionPreimage> {
    static ExprRef<BoolView> make(ExprRef<MSetView> l, ExprRef<MSetView> r);
};

ExprRef<BoolView> OpMaker<OpFunctionPreimage>::make(ExprRef<MSetView> l,
                                                ExprRef<MSetView> r) {
    return make_shared<OpFunctionPreimage>(move(l), move(r));
}
