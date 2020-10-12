#include "operators/opPartitionParty.h"

#include <iostream>
#include <memory>
#include "triggers/allTriggers.h"

#include "operators/simpleOperator.hpp"

using namespace std;
static const char* NO_PREIMAGE_UNDEFINED =
    "partition party does not yet support  undefined members.";

namespace {
HashType getHashForceDefined(const AnyExprRef& expr) {
    return lib::visit(
        [&](auto& expr) {
            auto view = expr->getViewIfDefined();
            if (!view) {
                myCerr << NO_PREIMAGE_UNDEFINED;
                myAbort();
            }
            return getValueHash(*view);
        },
        expr);
}

}  // namespace

template <typename OperandView>
void OpPartitionParty<OperandView>::reevaluateImpl(OperandView& partyMember,
                                                   PartitionView& partition,
                                                   bool, bool) {
    auto& partitionMembers = partition.getMembers<OperandView>();
    this->members.template emplace<ExprRefVec<viewType(partitionMembers)>>();
    this->silentClear();

    auto partyMemberHash = getValueHash(partyMember);
    auto iter = partition.hashIndexMap.find(partyMemberHash);
    if (iter != partition.hashIndexMap.end()) {
        cachedPart = partition.memberPartMap[iter->second];
        cachedPartitionMemberIndex = iter->second;
    } else {
        cachedPart = lib::nullopt;
        return;
    }
    for (size_t i = 0; i < partitionMembers.size(); i++) {
        if (partition.memberPartMap[i] == cachedPart.value()) {
            this->addMember(partitionMembers[i]);
        }
    }

    this->setAppearsDefined(true);
}

template <typename OperandView>
struct OperatorTrates<OpPartitionParty<OperandView>>::LeftTrigger
    : public ChangeTriggerAdapter<
          typename AssociatedTriggerType<OperandView>::type,
          OperatorTrates<OpPartitionParty<OperandView>>::LeftTrigger> {
    OpPartitionParty<OperandView>* op;

   public:
    LeftTrigger(OpPartitionParty<OperandView>* op)
        : ChangeTriggerAdapter<
              typename AssociatedTriggerType<OperandView>::type,
              OperatorTrates<OpPartitionParty<OperandView>>::LeftTrigger>(
              op->left),
          op(op) {}
    ExprRef<OperandView>& getTriggeringOperand() { return op->left; }
    void adapterValueChanged() {
        auto hash = getHashForceDefined(op->right);
        auto& view = op->right->getViewIfDefined().checkedGet("");
        auto iter = view.hashIndexMap.find(hash);
        lib::optional<UInt> newPartyIndex, newPartitionMemberIndex;
        if (iter != view.hashIndexMap.end()) {
            newPartitionMemberIndex = iter->second;
            newPartyIndex = view.memberPartMap[iter->second];
        }

        if (newPartitionMemberIndex == op->cachedPartitionMemberIndex ||
            newPartyIndex == op->cachedPart) {
            op->cachedPartitionMemberIndex = newPartitionMemberIndex;
            op->cachedPart = newPartyIndex;
            return;
        }
        op->reevaluate(true, true);
        op->notifyEntireValueChanged();
    }

    void adapterHasBecomeDefined() { todoImpl(); }

    void adapterHasBecomeUndefined() { todoImpl(); }

    void reattachTrigger() final {
        auto trigger = make_shared<
            OperatorTrates<OpPartitionParty<OperandView>>::LeftTrigger>(op);
        op->left->addTrigger(trigger);
        op->leftTrigger = trigger;
    }
};

template <typename OperandView>
struct OperatorTrates<OpPartitionParty<OperandView>>::RightTrigger
    : public PartitionTrigger {
    OpPartitionParty<OperandView>* op;

   public:
    RightTrigger(OpPartitionParty<OperandView>* op) : op(op) {}

    void containingPartsSwapped(UInt member1, UInt member2) {
        auto& view = op->right->getViewIfDefined().checkedGet("");
        if (!op->cachedPart ||
            (op->cachedPart.value() != view.memberPartMap[member1] &&
             op->cachedPart.value() != view.memberPartMap[member2])) {
            return;
        }
        if (view.memberPartMap[member1] == view.memberPartMap[member2]) {
            return;
        }
        if (member1 == op->cachedPartitionMemberIndex.value() ||
            member2 == op->cachedPartitionMemberIndex.value()) {
            op->reevaluate(true, true);
            op->notifyEntireValueChanged();
            return;
        }
        // find the member
        if (view.memberPartMap[member1] == op->cachedPart.value()) {
            // must be the other one
            swap(member1, member2);
        }
        debug_code(
            assert(view.memberPartMap[member2] == op->cachedPart.value()));
        auto hash = getHashForceDefined(
            view.template getMembers<OperandView>()[member1]);
        op->template removeMemberAndNotify<OperandView>(
            op->hashIndexMap.at(hash));
        op->addMemberAndNotify(
            view.template getMembers<OperandView>()[member2]);
    }
    // returns true if entire value reevaluated
    bool memberMoved(UInt index, UInt oldPart, UInt newPart,
                     PartitionView& view) {
        if (!op->cachedPart) {
            return false;
        }
        if (index == op->cachedPartitionMemberIndex.value()) {
            op->reevaluate(true, true);
            op->notifyEntireValueChanged();
            return true;
        }
        if (oldPart != op->cachedPart.value() &&
            newPart != op->cachedPart.value()) {
            return false;
        }
        if (oldPart == op->cachedPart.value() &&
            newPart != op->cachedPart.value()) {
            auto hash = getHashForceDefined(
                view.template getMembers<OperandView>()[index]);
            op->template removeMemberAndNotify<OperandView>(
                op->hashIndexMap.at(hash));
        } else if (oldPart != op->cachedPart.value() &&
                   newPart == op->cachedPart.value()) {
            op->addMemberAndNotify(
                view.template getMembers<OperandView>()[index]);
        }
        return false;
    }

    void membersMovedToPart(const std::vector<UInt>& memberIndices,
                            const std::vector<UInt>& oldParts, UInt destPart) {
        auto& view = op->right->getViewIfDefined().checkedGet("");
        for (size_t i = 0; i < memberIndices.size(); i++) {
            if (memberMoved(memberIndices[i], oldParts[i], destPart, view)) {
                break;
            }
        }
    }
    void membersMovedFromPart(UInt part, const std::vector<UInt>& memberIndices,
                              const std::vector<UInt>& newParts) {
        auto& view = op->right->getViewIfDefined().checkedGet("");
        for (size_t i = 0; i < memberIndices.size(); i++) {
            if (memberMoved(memberIndices[i], part, newParts[i], view)) {
                break;
            }
        }
    }
    void valueChanged() {
        op->reevaluate(true, true);
        op->notifyEntireValueChanged();
    }
    void memberReplaced(UInt, const AnyExprRef&) { todoImpl(); }

    void hasBecomeDefined() { todoImpl(); }
    void hasBecomeUndefined() { todoImpl(); }
    void memberHasBecomeDefined() { todoImpl(); }
    void memberHasBecomeUndefined() { todoImpl(); }

    void reattachTrigger() final {
        auto trigger = make_shared<
            OperatorTrates<OpPartitionParty<OperandView>>::RightTrigger>(op);
        op->right->addTrigger(trigger);
        op->rightTrigger = trigger;
    }
};
template <typename OperandView>
void OpPartitionParty<OperandView>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    this->left->updateVarViolations(vioContext, vioContainer);
    this->right->updateVarViolations(vioContext, vioContainer);
    if (cachedPart) {
        for (auto& member : this->template getMembers<OperandView>()) {
            member->updateVarViolations(vioContext, vioContainer);
        }
    }
}
template <typename OperandView>
void OpPartitionParty<OperandView>::copy(OpPartitionParty&) const {}
template <typename OperandView>
std::ostream& OpPartitionParty<OperandView>::dumpState(std::ostream& os) const {
    os << "OpPartitionParty: value=" << this->getViewIfDefined() << "\nLeft: ";
    this->left->dumpState(os);
    os << "\nRight: ";
    this->right->dumpState(os);
    return os;
}

template <typename OperandView>
string OpPartitionParty<OperandView>::getOpName() const {
    return toString(
        "OpPartitionParty<",
        TypeAsString<typename AssociatedValueType<OperandView>::type>::value,
        ">");
}
template <typename OperandView>
void OpPartitionParty<OperandView>::debugSanityCheckImpl() const {
    this->left->debugSanityCheck();
    this->right->debugSanityCheck();
    this->standardSanityChecksForThisType();
    auto leftOption = this->left->getViewIfDefined();
    auto rightOption = this->right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        sanityCheck(!this->appearsDefined(), "should be undefined.");
        return;
    }
    sanityCheck(this->appearsDefined(), "should be defined");
    auto& partyMember = *leftOption;
    auto& partition = *rightOption;
    auto& partitionMembers = partition.template getMembers<OperandView>();
    auto hash = getValueHash(partyMember);
    auto iter = partition.hashIndexMap.find(hash);
    if (iter != partition.hashIndexMap.end()) {
        sanityCheck(this->cachedPartitionMemberIndex.has_value(),
                    "should have value.");
        sanityCheck(this->cachedPart.has_value(), "should have value.");
        sanityEqualsCheck(iter->second, cachedPartitionMemberIndex.value());
        sanityEqualsCheck(partition.memberPartMap[iter->second],
                          cachedPart.value());
    } else {
        sanityCheck(!cachedPartitionMemberIndex.has_value(),
                    "should not have value");
        sanityCheck(!cachedPart.has_value(), "should not have value");
    }
    UInt count = 0;
    for (size_t i = 0; i < partitionMembers.size(); i++) {
        if (cachedPart && partition.memberPartMap[i] == cachedPart.value()) {
            count += 1;
            auto hash = getHashForceDefined(partitionMembers[i]);
            sanityCheck(
                this->hashIndexMap.count(hash),
                toString("member ", partitionMembers[i]->getViewIfDefined(),
                         " missing from party. "));
            sanityCheck(partitionMembers[i].getPtr().get() ==
                            this->template getMembers<
                                    OperandView>()[this->hashIndexMap.at(hash)]
                                .getPtr()
                                .get(),
                        "should be exact same member instance.");
        }
    }
    sanityEqualsCheck(count, this->numberElements());
}

template <typename Op>
struct OpMaker;

template <typename OperandView>
struct OpMaker<OpPartitionParty<OperandView>> {
    static ExprRef<SetView> make(ExprRef<OperandView> l,
                                 ExprRef<PartitionView> r);
};

template <typename OperandView>
ExprRef<SetView> OpMaker<OpPartitionParty<OperandView>>::make(
    ExprRef<OperandView> l, ExprRef<PartitionView> r) {
    return make_shared<OpPartitionParty<OperandView>>(move(l), move(r));
}

#define opMakerInstantiator(name)                                          \
    template ExprRef<SetView> OpMaker<OpPartitionParty<name##View>>::make( \
        ExprRef<name##View>, ExprRef<PartitionView>);
buildForAllTypes(opMakerInstantiator, );
#undef opMakerInstantiator
