#include "operators/opMSetLit.h"
#include <cassert>
#include <cstdlib>
#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpMSetLit::evaluateImpl() {
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member->evaluate();
                if (!member->appearsDefined()) {
                    myCerr << NO_MSET_UNDEFINED_MEMBERS;
                    myAbort();
                }
            }
        },
        members);
    this->setAppearsDefined(true);
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>,
      public OpMSetLit::ExprTriggerBase {
    typedef typename AssociatedViewType<TriggerType>::type View;
    using ExprTriggerBase::ExprTriggerBase;
    ExprRef<View>& getTriggeringOperand() {
        return op->getMembers<View>()[index];
    }
    ExprTrigger(OpMSetLit* op, UInt index)
        : ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>(
              op->getMembers<View>()[index]),
          ExprTriggerBase(op, index) {}
    void adapterValueChanged() { this->op->notifyMemberChanged(this->index); }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
            op->exprTriggers[index]));
        auto trigger = make_shared<ExprTrigger<TriggerType>>(op, index);
        op->getMembers<typename AssociatedViewType<TriggerType>::type>()[index]
            ->addTrigger(trigger);
        op->exprTriggers[index] = trigger;
    }

    void adapterHasBecomeDefined() {
        myCerr << NO_MSET_UNDEFINED_MEMBERS;
        myAbort();
    }

    void adapterHasBecomeUndefined() {
        myCerr << NO_MSET_UNDEFINED_MEMBERS;
        myAbort();
    }
};  // namespace
}  // namespace
void OpMSetLit::startTriggeringImpl() {
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

void OpMSetLit::stopTriggeringOnChildren() {
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

void OpMSetLit::stopTriggering() {
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

void OpMSetLit::updateVarViolationsImpl(const ViolationContext&,
                                        ViolationContainer&) {}

ExprRef<MSetView> OpMSetLit::deepCopyForUnrollImpl(
    const ExprRef<MSetView>&, const AnyIterRef& iterator) const {
    AnyExprVec newMembers;
    mpark::visit(
        [&](auto& members) {
            auto& newMembersImpl =
                newMembers.emplace<ExprRefVec<viewType(members)>>();
            for (auto& member : members) {
                newMembersImpl.emplace_back(
                    member->deepCopyForUnroll(member, iterator));
            }
        },
        members);

    auto newOpMSetLit = make_shared<OpMSetLit>(move(newMembers));
    return newOpMSetLit;
}

std::ostream& OpMSetLit::dumpState(std::ostream& os) const {
    os << "OpMSetLit: exprs=[";
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

void OpMSetLit::findAndReplaceSelf(const FindAndReplaceFunction& func) {
    mpark::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member = findAndReplace(member, func);
            }
        },
        members);
}

pair<bool, ExprRef<MSetView>> OpMSetLit::optimise(PathExtension path) {
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
    return make_pair(changeMade, mpark::get<ExprRef<MSetView>>(path.expr));
}

string OpMSetLit::getOpName() const { return "OpMSetLit"; }

void OpMSetLit::debugSanityCheckImpl() const {
    mpark::visit([&](auto& members) { recurseSanityChecks(members); }, members);
    this->standardSanityChecksForThisType();
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpMSetLit> {
    static ExprRef<MSetView> make(AnyExprVec members);
};

ExprRef<MSetView> OpMaker<OpMSetLit>::make(AnyExprVec o) {
    return make_shared<OpMSetLit>(move(o));
}
