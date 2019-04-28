#include "operators/quantifier.hpp"
#include "types/mSet.h"
using namespace std;
template <>
struct ContainerTrigger<MSetView> : public MSetTrigger, public DelayedTrigger {
    Quantifier<MSetView>* op;

    ContainerTrigger(Quantifier<MSetView>* op) : op(op) {}
    void valueRemoved(UInt indexOfRemovedValue, const AnyExprRef&) final {
        if (indexOfRemovedValue < op->numberUnrolled() - 1) {
            op->notifyContainerMembersSwapped(indexOfRemovedValue,
                                              op->numberUnrolled() - 1);
        }
        op->roll(op->numberUnrolled() - 1);
    }
    void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                typedef viewType(vToUnroll) View;
                vToUnroll.emplace_back(false, op->numberUnrolled(),
                                       mpark::get<ExprRef<View>>(member));
                if (!op->containerDelayedTrigger) {
                    op->containerDelayedTrigger =
                        make_shared<ContainerTrigger<MSetView>>(op);
                    addDelayedTrigger(op->containerDelayedTrigger);
                }
            },
            op->valuesToUnroll);
    }

    void memberValueChanged(UInt) final{};
    void memberValuesChanged(const std::vector<UInt>&) final{};

    void valueChanged() {
        while (op->numberUnrolled() != 0) {
            this->valueRemoved(op->numberUnrolled() - 1,
                               ExprRef<BoolView>(nullptr));
        }
        auto containerView = op->container->view();
        if (!containerView) {
            return;
        }
        mpark::visit(
            [&](auto& membersImpl) {
                for (auto& member : membersImpl) {
                    this->valueAdded(member);
                }
            },
            (*containerView).members);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                for (auto& value : vToUnroll) {
                    op->unroll(value);
                }
                vToUnroll.clear();
            },
            op->valuesToUnroll);
        deleteTrigger(op->containerDelayedTrigger);
        op->containerDelayedTrigger = nullptr;
    }
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<MSetView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }

    void hasBecomeUndefined() {
        op->containerDefined = false;
        if (op->appearsDefined()) {
            op->setAppearsDefined(false);
            op->notifyValueUndefined();
        }
    }
    void hasBecomeDefined() {
        this->valueChanged();
        op->containerDefined = true;
        op->setAppearsDefined(op->numberUndefined == 0);
        op->notifyValueDefined();
    }
};

template <>
struct InitialUnroller<MSetView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier, MSetView& containerView) {
        mpark::visit(
            [&](auto& membersImpl) {
                typedef viewType(membersImpl) View;
                quantifier.valuesToUnroll
                    .template emplace<QueuedUnrollValueVec<View>>();
                for (size_t i = 0; i < membersImpl.size(); i++) {
                    auto& member = membersImpl[i];
                    quantifier.template unroll<View>(
                        {false, i, member});
                }
            },
            containerView.members);
    }
};

template <>
struct ContainerSanityChecker<MSetView> {
    static void debugSanityCheck(const Quantifier<MSetView>& quant,
                                 const MSetView& container) {
        sanityEqualsCheck(container.numberElements(), quant.numberUnrolled());
        sanityEqualsCheck(container.numberElements(),
                          quant.unrolledIterVals.size());
        mpark::visit(
            [&](auto& containerMembers) {
                for (size_t i = 0; i < containerMembers.size(); i++) {
                    const auto& view1 = containerMembers[i]->getViewIfDefined();
                    const auto& view2 =
                        mpark::get<IterRef<viewType(containerMembers)>>(
                            quant.unrolledIterVals[i])
                            ->getViewIfDefined();
                    sanityEqualsCheck(view1.hasValue(), view2.hasValue());
                    if (view1.hasValue()) {
                        sanityEqualsCheck(getValueHash(*view1),
                                          getValueHash(*view2));
                    }
                }
            },
            quant.container->view()->members);
    }
};

template struct Quantifier<MSetView>;
