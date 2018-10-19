#include "operators/opSequenceLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpSequenceLit::evaluateImpl() {
    numberUndefined = 0;
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member->evaluate();
                if (!member->appearsDefined()) {
                    numberUndefined++;
                }
            }
        },
        members);
    this->setAppearsDefined(numberUndefined == 0);
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public OpSequenceLit::ExprTriggerBase,
      public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type View;
    using ExprTriggerBase::ExprTriggerBase;
    ExprRef<View>& getTriggeringOperand() {
        return op->getMembers<View>()[index];
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

void OpSequenceLit::startTriggeringImpl() {
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

void OpSequenceLit::updateVarViolationsImpl(const ViolationContext&,
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

pair<bool, ExprRef<SequenceView>> OpSequenceLit::optimise(PathExtension path) {
    bool changeMade = false;
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                auto optResult = member->optimise(path.extend(member));
                changeMade |= optResult.first;
                member = optResult.second;
            }
        },
        this->members);
    return make_pair(changeMade, mpark::get<ExprRef<SequenceView>>(path.expr));
}

string OpSequenceLit::getOpName() const { return "OpSequenceLit"; }

void OpSequenceLit::debugSanityCheckImpl() const {
    mpark::visit(
        [&](auto& members) {
            UInt checkNumberUndefined = 0;
            for (auto& member : members) {
                member->debugSanityCheck();
                if (member->getViewIfDefined().hasValue()) {
                    ++checkNumberUndefined;
                }
            }
            sanityEqualsCheck(checkNumberUndefined, numberUndefined);
        },
        this->members);
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
