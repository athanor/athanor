#include "operators/opTupleIndex.h"
#include "operators/opTupleLit.h"
#include "operators/quantifier.hpp"
#include "types/function.h"

template <typename Op>
struct OpMaker;
template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(std::vector<AnyExprRef> members);
};

using namespace std;

template <>
struct InitialUnroller<FunctionView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier, FunctionView& containerView) {
        mpark::visit(
            [&](auto& membersImpl) {
                for (size_t i = 0; i < membersImpl.size(); i++) {
                    if (containerView.dimensions.size() == 1) {
                        auto tupleFirstMember =
                            containerView.template indexToDomain<IntView>(i);
                        auto unrolledExpr = OpMaker<OpTupleLit>::make(
                            {tupleFirstMember, membersImpl[i]});
                        quantifier.unroll(i, unrolledExpr);
                    } else {
                        auto tupleFirstMember =
                            containerView.template indexToDomain<TupleView>(i);
                        auto unrolledExpr = OpMaker<OpTupleLit>::make(
                            {tupleFirstMember, membersImpl[i]});
                        quantifier.unroll(i, unrolledExpr);
                    }
                }
            },
            containerView.range);
    }
};

template <>
struct ContainerTrigger<FunctionView> : public FunctionTrigger {
    Quantifier<FunctionView>* op;

    ContainerTrigger(Quantifier<FunctionView>* op) : op(op) {}

    void imageChanged(UInt) final {}

    void imageChanged(const std::vector<UInt>&) final {}

    void valueChanged() {
        while (op->numberElements() != 0) {
            op->roll(op->numberElements() - 1);
        }
        auto view = op->container->getViewIfDefined();
        if (!view) {
        }
        InitialUnroller<FunctionView>::initialUnroll(*op, *view);
    }

    void imageSwap(UInt index1, UInt index2) final {
        op->swapExprs(index1, index2);
        correctUnrolledTupleIndex(index1);
        correctUnrolledTupleIndex(index2);
    }

    void correctUnrolledTupleIndex(size_t index) {
        debug_log("Correcting tuple index " << index);
        auto& tuple =
            mpark::get<IterRef<TupleView>>(op->unrolledIterVals[index]);
        if (op->container->view()->dimensions.size() == 1) {
            auto& preImage =
                mpark::get<ExprRef<IntView>>(tuple->view()->members[0]);
            op->container->view()->indexToDomain(index, (*preImage->view()));
        } else {
            auto& preImage =
                mpark::get<ExprRef<TupleView>>(tuple->view()->members[0]);
            op->container->view()->indexToDomain(index, (*preImage->view()));
        }
    }

    void hasBecomeUndefined() {
        op->containerDefined = false;
        if (op->appearsDefined()) {
            op->setAppearsDefined(false);
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers, true);
        }
    }
    void hasBecomeDefined() {
        this->valueChanged();
        op->containerDefined = true;
        op->setAppearsDefined(op->numberUndefined == 0);
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers,
                      true);
    }
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<FunctionView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }
};

template <>
struct ContainerSanityChecker<FunctionView> {
    static void debugSanityCheck(const Quantifier<FunctionView>& quant,
                                 const FunctionView& container) {
        sanityEqualsCheck(container.rangeSize(), quant.numberElements());
    }
};

template struct Quantifier<FunctionView>;
