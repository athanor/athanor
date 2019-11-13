#include "operators/opTupleLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;

void OpTupleLit::evaluateImpl() {
    numberUndefined = 0;
    for (auto& member : members) {
        lib::visit(
            [&](auto& member) {
                member->evaluate();
                if (!member->appearsDefined()) {
                    ++numberUndefined;
                }
            },
            member);
    }
    this->setAppearsDefined(this->numberUndefined == 0);
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>,
      OpTupleLit::ExprTriggerBase {
    typedef typename AssociatedViewType<TriggerType>::type View;
    using ExprTriggerBase::ExprTriggerBase;

    ExprTrigger(OpTupleLit* op, UInt index)
        : ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>(
              lib::get<ExprRef<View>>(op->members[index])),
          ExprTriggerBase(op, index) {}

    ExprRef<View>& getTriggeringOperand() {
        return lib::get<ExprRef<View>>(this->op->members[this->index]);
    }
    void adapterValueChanged() {
        this->op->memberChangedAndNotify(this->index);
    }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
            op->exprTriggers[index]));
        auto trigger = make_shared<ExprTrigger<TriggerType>>(op, index);
        lib::get<ExprRef<typename AssociatedViewType<TriggerType>::type>>(
            op->members[index])
            ->addTrigger(trigger);
        op->exprTriggers[index] = trigger;
    }
    void adapterHasBecomeDefined() { op->defineMemberAndNotify(index); }
    void adapterHasBecomeUndefined() { op->undefineMemberAndNotify(index); }
};

}  // namespace

void OpTupleLit::replaceMember(UInt index, const AnyExprRef& newMember) {
    debug_code(assert(index < members.size()));
    auto oldMember = move(members[index]);
    members[index] = move(newMember);
    lib::visit(
        [&](auto& member) {
            typedef viewType(member) View;
            typedef typename AssociatedTriggerType<View>::type TriggerType;

            if (index < exprTriggers.size()) {
                deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
                    exprTriggers[index]));
                auto trigger =
                    make_shared<ExprTrigger<TriggerType>>(this, index);
                member->addTrigger(trigger);
                exprTriggers[index] = move(trigger);
            }
        },
        members[index]);
    notifyMemberReplaced(index, oldMember);
}

void OpTupleLit::startTriggeringImpl() {
    if (exprTriggers.empty()) {
        for (size_t i = 0; i < members.size(); i++) {
            lib::visit(
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
            lib::visit(
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
            lib::visit([&](auto& member) { member->stopTriggering(); }, member);
        }
    }
}

void OpTupleLit::updateVarViolationsImpl(const ViolationContext&,
                                         ViolationContainer&) {}

ExprRef<TupleView> OpTupleLit::deepCopyForUnrollImpl(
    const ExprRef<TupleView>&, const AnyIterRef& iterator) const {
    vector<AnyExprRef> newMembers;
    for (auto& member : members) {
        lib::visit(
            [&](auto& member) {
                newMembers.emplace_back(
                    member->deepCopyForUnroll(member, iterator));
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
        lib::visit([&](auto& member) { member->dumpState(os); }, member);
    }
    os << ")";
    return os;
}

void OpTupleLit::findAndReplaceSelf(const FindAndReplaceFunction& func,
                                    PathExtension path) {
    for (auto& member : members) {
        lib::visit(
            [&](auto& member) { member = findAndReplace(member, func, path); },
            member);
    }
}

pair<bool, ExprRef<TupleView>> OpTupleLit::optimiseImpl(ExprRef<TupleView>&,
                                                        PathExtension path) {
    auto newOp = make_shared<OpTupleLit>(members);
    AnyExprRef newOpAsExpr = ExprRef<TupleView>(newOp);
    bool optimised = false;
    for (auto& member : newOp->members) {
        lib::visit(
            [&](auto& operand) {
                optimised |= optimise(newOpAsExpr, operand, path);
            },
            member);
    }
    return std::make_pair(optimised, newOp);
}

string OpTupleLit::getOpName() const { return "OpTupleLit"; }

void OpTupleLit::debugSanityCheckImpl() const {
    UInt checkNumberUndefined = 0;
    for (auto& member : members) {
        lib::visit(
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
