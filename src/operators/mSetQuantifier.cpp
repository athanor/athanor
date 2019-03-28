#include "operators/quantifier.hpp"
#include "types/mSet.h"
using namespace std;
template <>
struct ContainerTrigger<MSetView> : public MSetTrigger, public DelayedTrigger {
    Quantifier<MSetView>* op;

    ContainerTrigger(Quantifier<MSetView>* op) : op(op) {}
    void valueRemoved(UInt indexOfRemovedValue, const AnyExprRef&) final {
        if (indexOfRemovedValue < op->numberElements() - 1) {
            op->swapExprs(indexOfRemovedValue, op->numberElements() - 1);
        }
        op->roll(op->numberElements() - 1);
    }
    void valueAdded(const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<viewType(vToUnroll)>>(member));
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
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1,
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
                    op->unroll(op->numberElements(), value);
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
                for (auto& member : membersImpl) {
                    quantifier.unroll(quantifier.numberElements(), member);
                }
            },
            containerView.members);
    }
};

template <>
struct ContainerSanityChecker<MSetView> {
    static void debugSanityCheck(const Quantifier<MSetView>& quant,
                                 const MSetView& container) {
        sanityEqualsCheck(container.numberElements(), quant.numberElements());
    }
};

template struct Quantifier<MSetView>;
