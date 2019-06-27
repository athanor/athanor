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
                    mpark::visit(
                        [&](auto& fromDomain) {
                            typedef typename BaseType<decltype(
                                fromDomain)>::element_type Domain;
                            auto tupleFirstMember =
                                containerView.template indexToDomain<Domain>(i);
                            auto unrolledExpr = OpMaker<OpTupleLit>::make(
                                {tupleFirstMember, membersImpl[i]});
                            unrolledExpr->evaluate();
                            quantifier.template unroll<TupleView>(
                                {false, i, unrolledExpr});
                        },
                        containerView.fromDomain);
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
        while (op->numberUnrolled() != 0) {
            op->roll(op->numberUnrolled() - 1);
        }
        auto view = op->container->getViewIfDefined();
        if (!view) {
        }
        InitialUnroller<FunctionView>::initialUnroll(*op, *view);
    }

    void imageSwap(UInt index1, UInt index2) final {
        op->notifyContainerMembersSwapped(index1, index2);
        correctUnrolledTupleIndex(index1);
        correctUnrolledTupleIndex(index2);
    }

    void correctUnrolledTupleIndex(size_t index) {
        debug_log("Correcting tuple index " << index);
        auto& tuple =
            mpark::get<IterRef<TupleView>>(op->unrolledIterVals[index]);
        auto& containerView = *op->container->view();
        mpark::visit(
            [&](auto& fromDomain) {
                typedef typename BaseType<decltype(fromDomain)>::element_type
                    Domain;
                typedef typename AssociatedValueType<Domain>::type Value;
                typedef typename AssociatedViewType<Value>::type View;
                auto& preImage =
                    mpark::get<ExprRef<View>>(tuple->view()->members[0]);
                containerView.template indexToDomain<Domain>(
                    index, (*preImage->view()));
            },
            containerView.fromDomain);
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
        if (quant.condition) {
            sanityEqualsCheck(container.rangeSize(),
                              quant.unrolledConditions.size());
        } else {
            sanityEqualsCheck(container.rangeSize(), quant.numberElements());
        }

        sanityEqualsCheck(container.rangeSize(), quant.unrolledIterVals.size());
        for (size_t i = 0; i < quant.unrolledIterVals.size(); i++) {
            auto* iterPtr = mpark::get_if<IterRef<TupleView>>(
                &(quant.unrolledIterVals[i].asVariant()));
            sanityCheck(iterPtr, "Expected tuple type here.");
            auto view = (*iterPtr)->ref->view();
            sanityCheck(view, "view() should not return undefined here.");
            sanityEqualsCheck(2, view->members.size());
            mpark::visit(
                [&](const auto& fromDomain) {
                    typedef
                        typename BaseType<decltype(fromDomain)>::element_type
                            Domain;
                    typedef typename AssociatedValueType<Domain>::type Value;
                    typedef typename AssociatedViewType<Value>::type View;
                    auto tupleFirstMember =
                        container.template indexToDomain<Domain>(i);
                    auto* exprPtr =
                        mpark::get_if<ExprRef<View>>(&(view->members[0]));
                    sanityCheck(
                        exprPtr,
                        "Expected first element of unrolled tuple to be " +
                            TypeAsString<Value>::value);
                    auto memberView = (*exprPtr)->getViewIfDefined();
                    sanityCheck(
                        memberView,
                        "First member of unrolled tuple should be defined.");
                    sanityCheck(getValueHash(*memberView) ==
                                    getValueHash(*tupleFirstMember->view()),
                                toString("First member of tuple should be ",
                                         tupleFirstMember->view(),
                                         " but was actually ", memberView));
                },
                container.fromDomain);
        }
    }
};

template struct Quantifier<FunctionView>;
