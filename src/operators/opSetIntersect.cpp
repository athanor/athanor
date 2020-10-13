#include "operators/opSetIntersect.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/intVal.h"
#include "types/set.h"
using namespace std;
static const char* NO_UNDEFINED_IN_SETINTERSECT =
    "OpSetIntersect does not yet handle all the cases where sets become "
    "undefined.  "
    "Especially if returned views are undefined\n";

void OpSetIntersect::reevaluateImpl(SetView& leftView, SetView& rightView, bool,
                                    bool) {
    silentClear();
    mpark::visit(
        [&](auto& leftMembers) {
            this->members.emplace<ExprRefVec<viewType(leftMembers)>>();
            for (size_t i = 0; i < leftMembers.size(); i++) {
                if (rightView.hashIndexMap.count(leftView.indexHashMap[i])) {
                    this->addMember(leftMembers[i]);
                }
            }
        },
        leftView.members);
}

namespace {
HashType getHashForceDefined(const AnyExprRef& expr) {
    return lib::visit(
        [&](auto& expr) {
            auto view = expr->getViewIfDefined();
            if (!view) {
                myCerr << NO_UNDEFINED_IN_SETINTERSECT;
                myAbort();
            }
            return getValueHash(*view);
        },
        expr);
}

}  // namespace

void OpSetIntersect::valueRemoved(HashType hash) {
    mpark::visit(
        [&](auto& members) {
            if (hashIndexMap.count(hash)) {
                this->removeMemberAndNotify<viewType(members)>(
                    hashIndexMap.at(hash));
            }
        },
        this->members);
}

void OpSetIntersect::valueAdded(HashType hash) {
    auto& leftView =
        left->getViewIfDefined().checkedGet(NO_UNDEFINED_IN_SETINTERSECT);
    auto& rightView =
        right->getViewIfDefined().checkedGet(NO_UNDEFINED_IN_SETINTERSECT);
    auto iter = leftView.hashIndexMap.find(hash);
    if (iter == leftView.hashIndexMap.end()) {
        return;
    }
    if (!rightView.hashIndexMap.count(hash)) {
        return;
    }
    mpark::visit(
        [&](auto& leftMembers) {
            debug_code(assert(iter->second < leftMembers.size()));
            this->addMemberAndNotify(leftMembers[iter->second]);
        },
        leftView.members);
}
template <bool isLeft>
struct OperatorTrates<OpSetIntersect>::OperandTrigger : public SetTrigger {
   public:
    OpSetIntersect* op;

   public:
    OperandTrigger(OpSetIntersect* op) : op(op) {}
    void valueRemoved(UInt, HashType hash) { op->valueRemoved(hash); }
    void valueAdded(const AnyExprRef& member) {
        op->valueAdded(getHashForceDefined(member));
    }
    inline void valueChanged() final {
        op->reevaluate(isLeft, !isLeft);
        ;
        op->notifyEntireValueChanged();
    }
    void memberReplaced(UInt index, const AnyExprRef& oldMember) {
        op->valueRemoved(getHashForceDefined(oldMember));
        auto view = (isLeft) ? op->left->getViewIfDefined()
                             : op->right->getViewIfDefined();
        HashType hash =
            view.checkedGet(NO_UNDEFINED_IN_SETINTERSECT).indexHashMap[index];
        op->valueAdded(hash);
    }
    inline void memberValueChanged(UInt index, HashType oldHash) final {
        op->valueRemoved(oldHash);
        auto view = (isLeft) ? op->left->getViewIfDefined()
                             : op->right->getViewIfDefined();
        HashType hash =
            view.checkedGet(NO_UNDEFINED_IN_SETINTERSECT).indexHashMap[index];

        debug_log("adding new hash " << hash);
        op->valueAdded(hash);
    }

    inline void memberValuesChanged(
        const std::vector<UInt>& indices,
        const std::vector<HashType>& oldHashes) final {
        for (HashType hash : oldHashes) {
            op->valueRemoved(hash);
        }
        auto viewO = (isLeft) ? op->left->getViewIfDefined()
                              : op->right->getViewIfDefined();
        auto& view = viewO.checkedGet(NO_UNDEFINED_IN_SETINTERSECT);
        for (auto index : indices) {
            HashType hash = view.indexHashMap[index];
            op->valueAdded(hash);
        }
    }

    void reattachTrigger() final {
        if (isLeft) {
            reattachLeftTrigger();
        } else {
            reattachRightTrigger();
        }
    }
    void reattachLeftTrigger() {
        auto trigger =
            make_shared<OperatorTrates<OpSetIntersect>::OperandTrigger<true>>(
                op);
        op->left->addTrigger(trigger);
        op->leftTrigger = trigger;
    }

    void reattachRightTrigger() {
        auto trigger =
            make_shared<OperatorTrates<OpSetIntersect>::OperandTrigger<false>>(
                op);
        op->right->addTrigger(trigger);
        op->rightTrigger = trigger;
    }

    void hasBecomeUndefined() final { todoImpl(); }
    void hasBecomeDefined() final { todoImpl(); }
};

void OpSetIntersect::updateVarViolationsImpl(const ViolationContext& vioContext,
                                             ViolationContainer& vioContainer) {
    auto* intVioContextTest =
        dynamic_cast<const IntViolationContext*>(&vioContext);
    bool intersectionToLarge =
        intVioContextTest &&
        intVioContextTest->reason == IntViolationContext::Reason::TOO_LARGE;
    if (!intersectionToLarge) {
        left->updateVarViolations(vioContext, vioContainer);
        right->updateVarViolations(vioContext, vioContainer);
    } else {
        left->updateVarViolations(vioContext.parentViolation, vioContainer);
        right->updateVarViolations(vioContext.parentViolation, vioContainer);
        auto leftViewO = left->getViewIfDefined();
        auto rightViewO = right->getViewIfDefined();
        if (!leftViewO || !rightViewO) {
            return;
        }
        auto& leftView = *leftViewO;
        auto& rightView = *rightViewO;
        lib::visit(
            [&](auto& leftMembers) {
                auto& rightMembers =
                    lib::get<ExprRefVec<viewType(leftMembers)>>(
                        rightView.members);
                for (auto& hashIndexPair : this->hashIndexMap) {
                    auto hash = hashIndexPair.first;
                    auto leftIndex = leftView.hashIndexMap.at(hash);
                    auto rightIndex = rightView.hashIndexMap.at(hash);
                    leftMembers.at(leftIndex)->updateVarViolations(
                        vioContext.parentViolation, vioContainer);
                    rightMembers.at(rightIndex)
                        ->updateVarViolations(vioContext.parentViolation,
                                              vioContainer);
                }
            },
            leftView.members);
    }
}

void OpSetIntersect::copy(OpSetIntersect&) const {}

std::ostream& OpSetIntersect::dumpState(std::ostream& os) const {
    os << "OpSetIntersect: value=" << this->getViewIfDefined() << "\nLeft: ";
    left->dumpState(os);
    os << "\nRight: ";
    right->dumpState(os);
    return os;
}

string OpSetIntersect::getOpName() const { return "OpSetIntersect"; }
void OpSetIntersect::debugSanityCheckImpl() const {
    left->debugSanityCheck();
    right->debugSanityCheck();
    auto leftOption = left->getViewIfDefined();
    auto rightOption = right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        todoImpl();
        return;
    }
    auto& leftView = *leftOption;
    auto& rightView = *rightOption;
    standardSanityChecksForThisType();
    mpark::visit(
        [&](auto& leftMembers) {
            auto& opMembers =
                mpark::get<ExprRefVec<viewType(leftMembers)>>(this->members);
            size_t numberFound = 0;
            for (size_t i = 0; i < leftMembers.size(); i++) {
                HashType hash = leftView.indexHashMap[i];
                if (rightView.hashIndexMap.count(hash)) {
                    sanityCheck(
                        this->hashIndexMap.count(hash),
                        toString(
                            "left set member index ", i, " hash ", hash,
                            " is also in right set but is not in parent."));
                    sanityEqualsCheck(&(*leftMembers[i]),
                                      &(*opMembers[hashIndexMap.at(hash)]));
                    ++numberFound;
                }
            }
            sanityEqualsCheck(numberFound, numberElements());
        },
        leftView.members);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSetIntersect> {
    static ExprRef<SetView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};

ExprRef<SetView> OpMaker<OpSetIntersect>::make(ExprRef<SetView> l,
                                               ExprRef<SetView> r) {
    return make_shared<OpSetIntersect>(move(l), move(r));
}
