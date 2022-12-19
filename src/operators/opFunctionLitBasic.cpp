#include "operators/opFunctionLitBasic.h"

#include <cassert>
#include <cstdlib>

#include "operators/simpleOperator.hpp"
#include "triggers/allTriggers.h"
#include "utils/ignoreUnused.h"
using namespace std;
void OpFunctionLitBasic::evaluateImpl() {
    numberUndefined = 0;
    lib::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member->evaluate();
                if (!member->appearsDefined()) {
                    ++numberUndefined;
                }
            }
        },
        range);
    this->setAppearsDefined(numberUndefined == 0);
}

namespace {
template <typename TriggerType>
struct ExprTrigger
    : public ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>,
      public OpFunctionLitBasic::ExprTriggerBase {
    typedef typename AssociatedViewType<TriggerType>::type View;
    using ExprTriggerBase::ExprTriggerBase;
    ExprRef<View>& getTriggeringOperand() {
        return op->getRange<View>()[index];
    }
    ExprTrigger(OpFunctionLitBasic* op, UInt index)
        : ChangeTriggerAdapter<TriggerType, ExprTrigger<TriggerType>>(
              op->getMembers<View>()[index]),
          ExprTriggerBase(op, index) {}
    void adapterValueChanged() {
        this->op->template imageChanged<View>(this->index);
        this->op->notifyImageChanged(this->index);
    }

    void reattachTrigger() final {
        deleteTrigger(static_pointer_cast<ExprTrigger<TriggerType>>(
            op->exprTriggers[index]));
        auto trigger = make_shared<ExprTrigger<TriggerType>>(op, index);
        op->getRange<typename AssociatedViewType<TriggerType>::type>()[index]
            ->addTrigger(trigger);
        op->exprTriggers[index] = trigger;
    }

    void adapterHasBecomeDefined() {
        this->op->template defineMemberAndNotify<View>(this->index);
    }

    void adapterHasBecomeUndefined() {
        this->op->undefineMemberAndNotify(this->index);
    }
};
}  // namespace

void OpFunctionLitBasic::startTriggeringImpl() {
    if (exprTriggers.empty()) {
        lib::visit(
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
            range);
    }
}

void OpFunctionLitBasic::stopTriggeringOnChildren() {
    if (!exprTriggers.empty()) {
        lib::visit(
            [&](auto& members) {
                typedef typename AssociatedTriggerType<viewType(members)>::type
                    TriggerType;
                for (auto& trigger : exprTriggers) {
                    deleteTrigger(
                        static_pointer_cast<ExprTrigger<TriggerType>>(trigger));
                }
                exprTriggers.clear();
            },
            range);
    }
}

void OpFunctionLitBasic::stopTriggering() {
    if (!exprTriggers.empty()) {
        lib::visit(
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
            range);
    }
}

void OpFunctionLitBasic::updateVarViolationsImpl(const ViolationContext&,
                                                 ViolationContainer&) {}

ExprRef<FunctionView> OpFunctionLitBasic::deepCopyForUnrollImpl(
    const ExprRef<FunctionView>&, const AnyIterRef& iterator) const {
    AnyExprVec newMembers;
    lib::visit(
        [&](auto& members) {
            auto& newMembersImpl =
                newMembers.emplace<ExprRefVec<viewType(members)>>();
            for (auto& member : members) {
                newMembersImpl.emplace_back(
                    member->deepCopyForUnroll(member, iterator));
            }
        },
        range);

    auto newOpFunctionLitBasic = make_shared<OpFunctionLitBasic>();
    newOpFunctionLitBasic->initView(preimageDomain,
                                    makeDimensionVecFromDomain(preimageDomain),
                                    move(newMembers), false);
    return newOpFunctionLitBasic;
}

std::ostream& OpFunctionLitBasic::dumpState(std::ostream& os) const {
    os << "OpFunctionLitBasic(from " << preimageDomain
       << "): numberUndefined=" << numberUndefined
       << ", appearsDefined=" << this->appearsDefined() << ", range=(";
    lib::visit(
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
        range);
    os << ")";
    return os;
}

void OpFunctionLitBasic::findAndReplaceSelf(const FindAndReplaceFunction& func,
                                            PathExtension path) {
    lib::visit(
        [&](auto& members) {
            for (auto& member : members) {
                member = findAndReplace(member, func, path);
            }
        },
        range);
}

pair<bool, ExprRef<FunctionView>> OpFunctionLitBasic::optimiseImpl(
    ExprRef<FunctionView>&, PathExtension path) {
    auto newOp = make_shared<OpFunctionLitBasic>();
    newOp->initView(preimageDomain, preimages, range, partial);
    AnyExprRef newOpAsExpr = ExprRef<FunctionView>(newOp);
    bool optimised = false;
    lib::visit(
        [&](auto& operands) {
            for (auto& operand : operands) {
                optimised |= optimise(newOpAsExpr, operand, path);
            }
        },
        newOp->range);
    if (!newOp->lazyPreimages()) {
        lib::visit(
            [&](auto& operands) {
                for (auto& operand : operands) {
                    optimised |= optimise(newOpAsExpr, operand, path);
                }
            },
            newOp->getPreimages().preimages);
    }

    return std::make_pair(optimised, newOp);
}

string OpFunctionLitBasic::getOpName() const {
    return toString("OpFunctionLitBasic(from ", preimageDomain, ")");
}

void OpFunctionLitBasic::debugSanityCheckImpl() const {
    lib::visit([&](auto& members) { recurseSanityChecks(members); }, range);
    this->standardSanityChecksForThisType();
}

AnyExprVec& OpFunctionLitBasic::getChildrenOperands() { return range; }

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpFunctionLitBasic> {
    template <typename RangeViewType>
    static EnableIfViewAndReturn<RangeViewType, ExprRef<FunctionView>> make(
        AnyDomainRef preImageDomain);
};

template <typename RangeViewType>
EnableIfViewAndReturn<RangeViewType, ExprRef<FunctionView>>
OpMaker<OpFunctionLitBasic>::make(AnyDomainRef) {
    auto op = make_shared<OpFunctionLitBasic>();
    return op;
}

#define opMakerInstantiator(name)  \
    template ExprRef<FunctionView> \
        OpMaker<OpFunctionLitBasic>::make<name##View>(AnyDomainRef);
buildForAllTypes(opMakerInstantiator, );
#undef opMakerInstantiator
