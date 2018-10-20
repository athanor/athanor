#include "operators/opTupleLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpTupleLit::evaluateImpl() {
    for (auto& member : members) {
        mpark::visit(
            [&](auto& member) {
                member->evaluate();
                if (!member->appearsDefined()) {
                    ++numberUndefined;
                }
            },
            member);
    }
}
namespace {
template <typename TriggerType>
struct ExprTrigger
    : public OpTupleLit::ExprTriggerBase,
      public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>> {
    typedef typename AssociatedViewType<TriggerType>::type View;
    using ExprTriggerBase::ExprTriggerBase;
    ExprRef<View>& getTriggeringOperand() {
        return mpark::get<ExprRef<View>>(this->op->members[this->index]);
    }
    void adapterValueChanged() {
        this->op->memberChangedAndNotify(this->index);
    }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
            op->exprTriggers[index]));
        auto trigger = make_shared<ExprTrigger<TriggerType>>(op, index);
        mpark::get<ExprRef<typename AssociatedViewType<TriggerType>::type>>(
            op->members[index])
            ->addTrigger(trigger);
        op->exprTriggers[index] = trigger;
    }
    void adapterHasBecomeDefined() { op->notifyMemberDefined(index); }
    void adapterHasBecomeUndefined() { op->notifyMemberUndefined(index); }
};

}  // namespace
OpTupleLit::OpTupleLit(OpTupleLit&& other)
    : TupleView(std::move(other)), exprTriggers(std::move(other.exprTriggers)) {
    setTriggerParent(this, exprTriggers);
}

void OpTupleLit::startTriggeringImpl() {
    if (exprTriggers.empty()) {
        for (size_t i = 0; i < members.size(); i++) {
            mpark::visit(
                [&](auto& member) {
                    typedef
                        typename AssociatedTriggerType<viewType(member)>::type
                            TriggerType;

                    auto trigger =
                        make_shared<ExprTrigger<TriggerType>>(this, i);
                    member->addTrigger(trigger);
                    exprTriggers.emplace_back(move(trigger));
                    member->startTriggering();
                },
                members[i]);
        }
    }
}

void OpTupleLit::stopTriggeringOnChildren() {
    if (!exprTriggers.empty()) {
        for (size_t i = 0; i < exprTriggers.size(); i++) {
            auto& trigger = exprTriggers[i];
            mpark::visit(
                [&](auto& member) {
                    typedef
                        typename AssociatedTriggerType<viewType(member)>::type
                            TriggerType;
                    deleteTrigger(
                        static_pointer_cast<ExprTrigger<TriggerType>>(trigger));
                },
                members[i]);
        }
        exprTriggers.clear();
    }
}

void OpTupleLit::stopTriggering() {
    if (!exprTriggers.empty()) {
        this->stopTriggeringOnChildren();
        for (auto& member : members) {
            mpark::visit([&](auto& member) { member->stopTriggering(); },
                         member);
        }
    }
}

void OpTupleLit::updateVarViolationsImpl(const ViolationContext&,
                                         ViolationContainer&) {}

ExprRef<TupleView> OpTupleLit::deepCopySelfForUnrollImpl(
    const ExprRef<TupleView>&, const AnyIterRef& iterator) const {
    vector<AnyExprRef> newMembers;
    for (auto& member : members) {
        mpark::visit(
            [&](auto& member) {
                newMembers.emplace_back(
                    member->deepCopySelfForUnroll(member, iterator));
            },
            member);
    }
    auto newOpTupleLit = make_shared<OpTupleLit>(move(newMembers));
    newOpTupleLit->numberUndefined = numberUndefined;
    return newOpTupleLit;
}

std::ostream& OpTupleLit::dumpState(std::ostream& os) const {
    os << "OpTupleLit: (";
    bool first = true;
    for (const auto& member : members) {
        if (first) {
            first = false;
        } else {
            os << ",";
        }
        mpark::visit([&](auto& member) { member->dumpState(os); }, member);
    }
    os << ")";
    return os;
}

void OpTupleLit::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    for (auto& member : members) {
        mpark::visit(
            [&](auto& member) { member = findAndReplace(member, func); },
            member);
    }
}

pair<bool, ExprRef<TupleView>> OpTupleLit::optimise(PathExtension path) {
    bool changeMade = false;
    for (auto& member : members) {
        mpark::visit(
            [&](auto& member) {
                auto optResult = member->optimise(path.extend(member));
                changeMade |= optResult.first;
                member = optResult.second;
            },
            member);
    }
    return make_pair(changeMade, mpark::get<ExprRef<TupleView>>(path.expr));
}
string OpTupleLit::getOpName() const { return "OpTupleLit"; }

void OpTupleLit::debugSanityCheckImpl() const {
    UInt checkNumberUndefined = 0;
    for (auto& member : members) {
        mpark::visit(
            [&](auto& member) {
                member->debugSanityCheck();
                if (!member->getViewIfDefined().hasValue()) {
                    ++checkNumberUndefined;
                }
            },
            member);
    }
    sanityEqualsCheck(checkNumberUndefined, numberUndefined);
    if (numberUndefined == 0) {
        sanityCheck(this->appearsDefined(), "operator should be defined.");
    } else {
        sanityCheck(!this->appearsDefined(), "operator should be undefined.");
    }
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(vector<AnyExprRef> members);
};

ExprRef<TupleView> OpMaker<OpTupleLit>::make(vector<AnyExprRef> o) {
    return make_shared<OpTupleLit>(move(o));
}
