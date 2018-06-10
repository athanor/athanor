#include "operators/opSequenceLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/simpleOperator.hpp"
#include "types/allTypes.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpSequenceLit::evaluateImpl() {
    numberUndefined = 0;
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member->evaluate();
                if (member->isUndefined()) {
                    numberUndefined++;
                }
            }
        },
        members);
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public OpSequenceLit::ExprTriggerBase,
      public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type View;
    using ExprTriggerBase::ExprTriggerBase;
    void adapterPossibleValueChange() {
        this->op->template notifyPossibleSubsequenceChange<View>(
            this->index, this->index + 1);
    }
    void adapterValueChanged() {
        this->op->template changeSubsequenceAndNotify<View>(this->index,
                                                            this->index + 1);
    }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
            op->exprTriggers[index]));
        auto trigger = make_shared<ExprTrigger<TriggerType>>(op, index);
        op->getMembers<typename AssociatedViewType<TriggerType>::type>()[index]
            ->addTrigger(trigger);
        op->exprTriggers[index] = trigger;
    }

    void adapterHasBecomeDefined() {
        op->template notifyMemberDefined<View>(index);
    }

    void adapterHasBecomeUndefined() {
        op->template notifyMemberUndefined<View>(index);
    }
};  // namespace
}  // namespace
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

void OpSequenceLit::updateVarViolations(const ViolationContext&,
                                        ViolationContainer&) {}

ExprRef<SequenceView> OpSequenceLit::deepCopySelfForUnrollImpl(
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
    newOpSequenceLit->numberUndefined = numberUndefined;
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
                    os << ",\n";
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
bool OpSequenceLit::isUndefined() { return this->numberUndefined > 0; }
bool OpSequenceLit::optimise(PathExtension path) {
    bool changeMade = false;
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                changeMade |= member->optimise(path.extend(member));
            }
        },
        this->members);
    return changeMade;
}
template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpSequenceLit> {
    static ExprRef<SequenceView> make(AnyExprVec members);
};

ExprRef<SequenceView> OpMaker<OpSequenceLit>::make(AnyExprVec o) {
    return make_shared<OpSequenceLit>(move(o));
}
