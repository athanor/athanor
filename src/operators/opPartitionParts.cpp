#include "operators/opPartitionParts.h"

#include <cassert>
#include <cstdlib>

#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;

void OpPartitionParts::reevaluateImpl(PartitionView& operand) {
    partSetMap.clear();
    setPartMap.clear();
    mpark::visit(
        [&](auto& operandMembers) {
            typedef viewType(operandMembers) View;
            this->members.emplace<ExprRefVec<SetView>>();
            this->silentClear();
            partSetMap.resize(operandMembers.size(), -1);
            for (size_t i = 0; i < operandMembers.size(); i++) {
                auto part = operand.memberPartMap[i];
                if (partSetMap[part] == -1) {
                    auto newPart = make_shared<Part>();
                    newPart->members.emplace<ExprRefVec<View>>();
                    newPart->addMember(operandMembers[i]);
                    this->addMember<SetView>(newPart);
                    partSetMap[part] = this->numberElements() - 1;
                    setPartMap.emplace_back(part);
                } else {
                    auto existingPart = getAs<Part>(
                        this->getMembers<SetView>()[partSetMap[part]]);
                    existingPart->addMember(operandMembers[i]);
                }
            }
        },
        operand.members);
    this->setAppearsDefined(true);
}

struct OperatorTrates<OpPartitionParts>::OperandTrigger
    : public PartitionTrigger {
    OpPartitionParts* op;

    OperandTrigger(OpPartitionParts* op) : op(op) {}
    void valueChanged() final {
        op->reevaluate();
        op->notifyEntireValueChanged();
    }
    void hasBecomeUndefined() final { todoImpl(); }
    void hasBecomeDefined() final { todoImpl(); }
    //    void memberHasBecomeUndefined(UInt) final { todoImpl(); }
    //    void memberHasBecomeDefined(UInt) final { todoImpl(); }

    void reattachTrigger() final {
        auto trigger =
            make_shared<OperatorTrates<OpPartitionParts>::OperandTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }
    PartitionView& getOperandViewNoUndefined() {
        return op->operand->getViewIfDefined().checkedGet(
            "Not handling undefined at this point.");
    }
    inline HashType getPartitionMemberHash(PartitionView& view, UInt index) {
        return mpark::visit(
            [&](auto& members) { return getValueHash(members[index]); },
            view.members);
    }
    void containingPartsSwapped(UInt index1, UInt index2) {
        auto& view = getOperandViewNoUndefined();
        UInt part1Index = view.memberPartMap[index1];
        UInt part2Index = view.memberPartMap[index2];
        if (part1Index == part2Index) {
            return;
        }
        auto& part1 =
            *getAs<Part>(op->getMembers<SetView>()[op->partSetMap[part1Index]]);
        auto& part2 =
            *getAs<Part>(op->getMembers<SetView>()[op->partSetMap[part2Index]]);
        mpark::visit(
            [&](auto operandMembers) {
                typedef viewType(operandMembers) View;
                auto member1Hash = getValueHash(operandMembers[index1]);
                auto member2Hash = getValueHash(operandMembers[index2]);
                // now member1 should be in part2 and member2 in part1 as this
                // is how it was before the swap.
                debug_code(assert(part2.hashIndexMap.count(member1Hash)));
                debug_code(assert(part1.hashIndexMap.count(member2Hash)));
                auto member1 = part2.removeMemberAndNotify<View>(
                    part2.hashIndexMap[member1Hash]);
                auto member2 = part1.removeMemberAndNotify<View>(
                    part1.hashIndexMap[member2Hash]);
                debug_code(assert(member1.getPtr().get() ==
                                  operandMembers[index1].getPtr().get()));
                debug_code(assert(member2.getPtr().get() ==
                                  operandMembers[index2].getPtr().get()));

                part1.addMemberAndNotify(member1);
                part2.addMemberAndNotify(member2);
                op->membersChangedAndNotify<SetView>(
                    {(UInt)op->partSetMap[part1Index],
                     (UInt)op->partSetMap[part2Index]});
            },
            view.members);
    }

    void removePartAndNotify(UInt setIndex) {
        op->removeMemberAndNotify<SetView>(setIndex);
        // removed set, now update partSetMapping
        op->partSetMap[op->setPartMap[setIndex]] = -1;
        op->setPartMap[setIndex] = op->setPartMap.back();
        op->setPartMap.pop_back();
        if (setIndex < op->setPartMap.size()) {
            op->partSetMap[op->setPartMap[setIndex]] = setIndex;
        }
    }

    void addPartAndNotify(UInt partIndex, ExprRef<SetView> part) {
        op->addMemberAndNotify(part);
        op->partSetMap[partIndex] = op->numberElements() - 1;
        op->setPartMap.emplace_back(partIndex);
    }
    void membersMovedToPart(const std::vector<UInt>& memberIndices,
                            const std::vector<UInt>& oldParts, UInt destPart) {
        auto& view = getOperandViewNoUndefined();
        mpark::visit(
            [&](auto& operandMembers) {
                typedef viewType(operandMembers) View;

                auto destSetIndex = op->partSetMap[destPart];
                bool newSet = destSetIndex == -1;
                auto part = (newSet) ? ExprRef<SetView>(make_shared<Part>())
                                     : op->getMembers<SetView>()[destSetIndex];
                auto& partView = *getAs<Part>(part);
                if (newSet) {
                    partView.members.emplace<ExprRefVec<View>>();
                }

                for (size_t i = 0; i < memberIndices.size(); i++) {
                    auto memberIndex = memberIndices[i];
                    auto memberPart = oldParts[i];
                    auto hash = getValueHash(operandMembers[memberIndex]);
                    auto setIndex = op->partSetMap[memberPart];
                    auto& set =
                        *getAs<Part>(op->getMembers<SetView>()[setIndex]);
                    debug_code(assert(set.hashIndexMap.count(hash)));
                    auto member =
                        set.removeMemberAndNotify<View>(set.hashIndexMap[hash]);
                    debug_code(
                        assert(member.getPtr().get() ==
                               operandMembers[memberIndex].getPtr().get()));
                    op->memberChangedAndNotify<SetView>(setIndex);
                    if (set.numberElements() == 0) {
                        removePartAndNotify(setIndex);
                        if (!newSet &&
                            destSetIndex == (Int)op->setPartMap.size()) {
                            destSetIndex = setIndex;
                        }
                    }
                    partView.addMemberAndNotify(member);
                    if (!newSet) {
                        op->memberChangedAndNotify<SetView>(destSetIndex);
                    }
                }
                if (newSet) {
                    addPartAndNotify(destPart, part);
                }
            },
            view.members);
    }
    void membersMovedFromPart(UInt part, const std::vector<UInt>& memberIndices,
                              const std::vector<UInt>& newParts) {
        auto& view = getOperandViewNoUndefined();
        mpark::visit(
            [&](auto& operandMembers) {
                typedef viewType(operandMembers) View;
                auto sourceSetIndex = op->partSetMap[part];
                auto sourceSet = op->getMembers<SetView>()[sourceSetIndex];
                auto& sourceSetView = *getAs<Part>(sourceSet);
                bool deleteSourceSet =
                    sourceSetView.numberElements() == memberIndices.size();
                if (deleteSourceSet) {
                    removePartAndNotify(sourceSetIndex);
                }
                for (size_t i = 0; i < memberIndices.size(); i++) {
                    auto memberIndex = memberIndices[i];
                    auto destPart = newParts[i];
                    auto memberHash = getValueHash(operandMembers[memberIndex]);
                    debug_code(
                        assert(sourceSetView.hashIndexMap.count(memberHash)));
                    auto member = sourceSetView.removeMemberAndNotify<View>(
                        sourceSetView.hashIndexMap[memberHash]);

                    debug_code(
                        assert(member.getPtr().get() ==
                               operandMembers[memberIndex].getPtr().get()));
                    if (!deleteSourceSet) {
                        op->memberChangedAndNotify<SetView>(sourceSetIndex);
                    }
                    auto destSetIndex = op->partSetMap[destPart];
                    if (destSetIndex == -1) {
                        auto part = make_shared<Part>();
                        part->members.emplace<ExprRefVec<View>>();
                        part->addMemberAndNotify(member);
                        addPartAndNotify(destPart, ExprRef<SetView>(part));
                    } else {
                        getAs<Part>(op->getMembers<SetView>()[destSetIndex])
                            ->addMemberAndNotify(member);
                        op->memberChangedAndNotify<SetView>(destSetIndex);
                    }
                }
            },
            view.members);
    }
    void memberReplaced(UInt sourceIndex, const AnyExprRef& oldMember) final {
        todoImpl(sourceIndex, oldMember);
    }
};

void OpPartitionParts::updateVarViolationsImpl(const ViolationContext&,
                                               ViolationContainer&) {}

void OpPartitionParts::copy(OpPartitionParts&) const {}

std::ostream& OpPartitionParts::dumpState(std::ostream& os) const {
    os << "OpPartitionParts: value=" << this->getViewIfDefined()
       << "\npartSetMap: " << partSetMap << "\nsetPartMap: " << setPartMap
       << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpPartitionParts::getOpName() const { return "OpPartitionParts"; }
void OpPartitionParts::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    auto viewOption = operand->getViewIfDefined();
    if (!viewOption) {
        sanityCheck(!this->appearsDefined(), "should be undefined");
    }
    sanityCheck(this->appearsDefined(), "should be defined.");
    auto& view = *viewOption;
    // first check internal consistency of partSetMap and setPartMap
    sanityEqualsCheck(view.numberElements(), this->partSetMap.size());
    sanityEqualsCheck(view.numberParts(), this->setPartMap.size());
    sanityEqualsCheck(this->numberElements(), this->setPartMap.size());
    for (Int part = 0; part < (Int)partSetMap.size(); part++) {
        auto setIndex = partSetMap[part];
        if (view.partInfo[part].partSize == 0) {
            sanityEqualsCheck(-1, setIndex);
        } else {
            sanityCheck(
                setIndex < (Int)setPartMap.size(),
                toString("set index for partSetMap[", part, "] out of range."));
            sanityCheck(setIndex >= 0,
                        toString("set index for partSetMap[", part,
                                 "]  should be greater equals 0."));
            sanityEqualsCheck(part, (Int)setPartMap[setIndex]);
            sanityEqualsCheck(view.partInfo[part].partSize,
                              getAs<Part>(this->getMembers<SetView>()[setIndex])
                                  ->numberElements());
        }
    }

    mpark::visit(
        [&](auto& operandMembers) {
            typedef viewType(operandMembers) View;
            for (size_t i = 0; i < operandMembers.size(); i++) {
                auto member = operandMembers[i];
                auto partIndex = view.memberPartMap[i];
                auto setIndex = partSetMap[partIndex];
                auto memberHash = getValueHash(member);
                auto& set = *getAs<Part>(this->getMembers<SetView>()[setIndex]);
                sanityCheck(set.hashIndexMap.count(memberHash),
                            toString("hash ", memberHash,
                                     " Hmissing from set index ", setIndex));
                auto& checkMember =
                    set.getMembers<View>()[set.hashIndexMap.at(memberHash)];
                sanityEqualsCheck(member.getPtr().get(),
                                  checkMember.getPtr().get());
            }
        },
        view.members);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpPartitionParts> {
    static ExprRef<SetView> make(ExprRef<PartitionView> o);
};

ExprRef<SetView> OpMaker<OpPartitionParts>::make(ExprRef<PartitionView> o) {
    return make_shared<OpPartitionParts>(move(o));
}
