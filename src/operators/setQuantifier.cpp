#include "operators/quantifier.hpp"
#include "types/set.h"
using namespace std;

template <>
struct ContainerTrigger<SetView> : public SetTrigger {
    Quantifier<SetView>* op;

    ContainerTrigger(Quantifier<SetView>* op) : op(op) {}
    void valueRemoved(UInt indexOfRemovedValue, HashType) final {
        if (indexOfRemovedValue < op->numberUnrolled() - 1) {
            op->notifyContainerMembersSwapped(indexOfRemovedValue,
                                              op->numberUnrolled() - 1);
        }
        op->roll(op->numberUnrolled() - 1);
    }

    void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& expr) {
                typedef viewType(expr) View;
                op->unroll<View>({false, op->numberUnrolled(), expr});
            },
            member);
    }

    void memberReplaced(UInt index, const AnyExprRef& oldMember) {
        debug_code(assert(index < op->unrolledIterVals.size()));
        auto& containerView = *op->container->view();

        mpark::visit(
            [&](auto& oldMember) {
                typedef viewType(oldMember) View;
                auto iterRef =
                    mpark::get<IterRef<View>>(op->unrolledIterVals[index]);
                auto& newMember = containerView.getMembers<View>()[index];
                iterRef->changeValueAndTrigger(newMember);
            },
            oldMember);
    }

    void memberValueChanged(UInt, HashType) final{};
    void memberValuesChanged(const std::vector<UInt>&,
                             const vector<HashType>&) final{};

    void valueChanged() {
        while (op->numberUnrolled() != 0) {
            this->valueRemoved(op->numberUnrolled() - 1, HashType(0));
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
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<SetView>>(op);
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
struct InitialUnroller<SetView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier, SetView& containerView) {
        mpark::visit(
            [&](auto& containerMembers) {
                for (size_t i = 0; i < containerMembers.size(); i++) {
                    auto& member = containerMembers[i];
                    quantifier.template unroll<viewType(member)>(
                        {false, i, member});
                }
            },
            containerView.members);
    }
};

template <>
struct ContainerSanityChecker<SetView> {
    static void debugSanityCheck(const Quantifier<SetView>& quant,
                                 const SetView& container) {
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

template struct Quantifier<SetView>;
