#include "operators/opFunctionDefined.h"
#include <iostream>
#include <memory>
#include "operators/simpleOperator.hpp"
#include "types/set.h"

using namespace std;

void OpFunctionDefined::reevaluateImpl(FunctionView& operandView) {
    debug_log("evaluating");
    lib::visit(
        [&](auto& preimageDomainPtr) {
            typedef typename BaseType<decltype(preimageDomainPtr)>::element_type
                PreimageDomain;
            typedef typename AssociatedViewType<PreimageDomain>::type
                PreimageViewType;
            this->members.emplace<ExprRefVec<PreimageViewType>>();
            this->silentClear();
            for (size_t i = 0; i < operandView.rangeSize(); i++) {
                this->addMember(operandView.indexToPreimage<PreimageDomain>(i));
            }
        },
        operandView.preimageDomain);
    this->setAppearsDefined(true);
    debug_log("evaluated to " << this->getViewIfDefined());
}

struct OperatorTrates<OpFunctionDefined>::OperandTrigger
    : public FunctionTrigger {
    OpFunctionDefined* op;
    OperandTrigger(OpFunctionDefined* op) : op(op) {}

    void valueRemoved(UInt indexOfRemovedValue, const AnyExprRef&,
                      const AnyExprRef&) final {
        lib::visit(
            [&](auto& members) {
                op->removeMemberAndNotify<viewType(members)>(
                    indexOfRemovedValue);
            },
            op->members);
    }
    void valueAdded(const AnyExprRef& preimage, const AnyExprRef&) final {
        lib::visit([&](auto& preimage) { op->addMemberAndNotify(preimage); },
                   preimage);
    }
    void memberHasBecomeDefined(UInt) final {}
    void memberHasBecomeUndefined(UInt) final {}
    void imageChanged(UInt) final {}
    void imageChanged(const std::vector<UInt>&) final {}
    void imageSwap(UInt, UInt) final {}
    void preimageChanged(UInt index, HashType) final {
        lib::visit(
            [&](auto& members) {
                op->memberChangedAndNotify<viewType(members)>(index);
            },
            op->members);
    }
    void preimageChanged(const std::vector<UInt>& indices,
                         const std::vector<HashType>&) final {
        lib::visit(
            [&](auto& members) {
                op->membersChangedAndNotify<viewType(members)>(indices);
            },
            op->members);
    }

    void valueChanged() {
        op->reevaluate();
        op->notifyEntireValueChanged();
    }
    void reattachTrigger() {
        auto trigger =
            make_shared<OperatorTrates<OpFunctionDefined>::OperandTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }
    void hasBecomeUndefined() final { todoImpl(); }
    void hasBecomeDefined() final { todoImpl(); }
    void memberReplaced(UInt, const AnyExprRef&) {}
};

void OpFunctionDefined::updateVarViolationsImpl(const ViolationContext&,
                                                ViolationContainer&) {}

void OpFunctionDefined::copy(OpFunctionDefined&) const {}

std::ostream& OpFunctionDefined::dumpState(std::ostream& os) const {
    os << "OpFunctionDefined: value=" << this->getViewIfDefined()
       << "\noperand: ";
    operand->dumpState(os);
    return os;
}

string OpFunctionDefined::getOpName() const { return "OpFunctionDefined"; }
void OpFunctionDefined::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    this->standardSanityDefinednessChecks();
    auto view = operand->getViewIfDefined();
    if (!view) {
        sanityCheck(!this->appearsDefined(), "should be undefined.");
    }
    sanityCheck(this->appearsDefined(), "should be defined.");

    sanityEqualsCheck(view->rangeSize(), this->numberElements());
    lib::visit(
        [&](auto& members) {
            typedef typename AssociatedDomain<viewType(members)>::type
                PreimageDomain;
            for (size_t i = 0; i < members.size(); i++) {
                auto preimage = view->indexToPreimage<PreimageDomain>(i);
                if (view->lazyPreimages()) {
                    sanityEqualsCheck(getValueHash(preimage),
                                      getValueHash(members[i]));
                } else {
                    sanityEqualsCheck(preimage.getPtr().get(),
                                      members[i].getPtr().get());
                }
            }
        },
        this->members);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpFunctionDefined> {
    static ExprRef<SetView> make(ExprRef<FunctionView> o);
};

ExprRef<SetView> OpMaker<OpFunctionDefined>::make(ExprRef<FunctionView> o) {
    return make_shared<OpFunctionDefined>(move(o));
}
