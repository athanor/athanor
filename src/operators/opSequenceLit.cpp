#include "operators/opSequenceLit.h"
#include <cassert>
#include <cstdlib>
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpSequenceLit::evaluate() {
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member->evaluate();
            }
        },
        members);
}
template <typename TriggerType>
struct ExprTrigger
    : public OpSequenceLit::ExprTriggerBase,
      public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>> {
    using ExprTriggerBase::ExprTriggerBase;
    void adapterPossibleValueChange() {
        mpark::visit(
            [&](auto& members) {
                this->op->template notifyPossibleSubsequenceChange<viewType(
                    members)>(this->index, this->index + 1);
            },
            this->op->members);
    }
    void adapterValueChanged() {
        mpark::visit(
            [&](auto& members) {
                this->op
                    ->template changeSubsequenceAndNotify<viewType(members)>(
                        this->index, this->index + 1);
            },
            this->op->members);
    }
};

OpSequenceLit::OpSequenceLit(OpSequenceLit&& other)
    : SequenceView(std::move(other)),
      exprTriggers(std::move(other.exprTriggers)) {
    setTriggerParent(this, exprTriggers);
}

void OpSequenceLit::startTriggering() {
    if (exprTriggers.empty()) {
        mpark::visit(
            [&](auto& members) {
                typedef typename AssociatedTriggerType<viewType(members)>::type
                    TriggerType;
                for (size_t i = 0; i < members.size(); i++) {
                    auto trigger =
                        make_shared<ExprTrigger<TriggerType>>(this, i);
                    members[i]->addTrigger(trigger);
                    exprTriggers.emplace_back(move(trigger));
                    members[i]->startTriggering();
                }
            },
            members);
    }
}

void OpSequenceLit::stopTriggeringOnChildren() {
    if (!exprTriggers.empty()) {
        mpark::visit(
            [&](auto& members) {
                typedef typename AssociatedTriggerType<viewType(members)>::type
                    TriggerType;
                for (auto& trigger : exprTriggers) {
                    deleteTrigger(
                        static_pointer_cast<ExprTrigger<TriggerType>>(trigger));
                }
                exprTriggers.clear();
            },
            members);
    }
}

void OpSequenceLit::stopTriggering() {
    if (!exprTriggers.empty()) {
        mpark::visit(
            [&](auto& members) {
                typedef typename AssociatedTriggerType<viewType(members)>::type
                    TriggerType;
                for (auto& member : members) {
                    member->stopTriggering();
                }
                for (auto& trigger : exprTriggers) {
                    deleteTrigger(
                        static_pointer_cast<ExprTrigger<TriggerType>>(trigger));
                }
                exprTriggers.clear();
            },
            members);
    }
}

void OpSequenceLit::updateViolationDescription(UInt, ViolationDescription&) {}

ExprRef<SequenceView> OpSequenceLit::deepCopySelfForUnroll(
    const ExprRef<SequenceView>&, const AnyIterRef& iterator) const {
    AnyExprVec newMembers;
    mpark::visit(
        [&](auto& members) {
            auto& newMembersImpl =
                newMembers.emplace<ExprRefVec<viewType(members)>>();
            for (auto& member : members) {
                newMembersImpl.emplace_back(
                    member->deepCopySelfForUnroll(member, iterator));
            }
        },
        members);

    auto newOpSequenceLit = make_shared<OpSequenceLit>(move(newMembers));
    return newOpSequenceLit;
}

std::ostream& OpSequenceLit::dumpState(std::ostream& os) const {
    os << "OpSequenceLit: exprs=[";
    mpark::visit(
        [&](auto& members) {
            bool first = true;
            for (const auto& member : members) {
                if (first) {
                    first = false;
                } else {
                    os << ",";
                }
                member->dumpState(os);
            }
        },
        members);
    os << "]";
    return os;
}

void OpSequenceLit::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member = findAndReplace(member, func);
            }
        },
        members);
}
