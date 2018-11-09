#include "operators/opTupleIndex.h"
#include "operators/opTupleLit.h"
#include "operators/quantifier.hpp"
#include "types/sequence.h"

template <typename Op>
struct OpMaker;
template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(std::vector<AnyExprRef> members);
};

using namespace std;
template <>
struct ContainerTrigger<SequenceView> : public SequenceTrigger,
                                        public DelayedTrigger {
    Quantifier<SequenceView>* op;

    ContainerTrigger(Quantifier<SequenceView>* op) : op(op) {}
    void valueRemoved(UInt indexOfRemovedValue, const AnyExprRef&) final {
        op->roll(indexOfRemovedValue);
        correctUnrolledTupleIndices(indexOfRemovedValue);
    }
    void valueAdded(UInt index, const AnyExprRef& member) final {
        mpark::visit(
            [&](auto& vToUnroll) {
                vToUnroll.emplace_back(
                    mpark::get<ExprRef<viewType(vToUnroll)>>(member));
                op->indicesOfValuesToUnroll.emplace_back(index);
                if (!op->containerDelayedTrigger) {
                    op->containerDelayedTrigger =
                        make_shared<ContainerTrigger<SequenceView>>(op);
                    addDelayedTrigger(op->containerDelayedTrigger);
                }
            },
            op->valuesToUnroll);
    }

    void subsequenceChanged(UInt, UInt) final{};

    void valueChanged() {
        while (op->numberElements() != 0) {
            this->valueRemoved(op->numberElements() - 1,
                               ExprRef<BoolView>(nullptr));
        }
        op->indicesOfValuesToUnroll.clear();
        auto containerView = op->container->view();
        if (!containerView) {
            return;
        }
        mpark::visit(
            [&](auto& membersImpl) {
                auto& vToUnroll = mpark::get<ExprRefVec<viewType(membersImpl)>>(
                    op->valuesToUnroll);
                op->indicesOfValuesToUnroll.clear();
                vToUnroll.clear();
                for (auto& member : membersImpl) {
                    this->valueAdded(vToUnroll.size(), member);
                }
            },
            (*containerView).members);
    }

    void positionsSwapped(UInt index1, UInt index2) final {
        op->swapExprs(index1, index2);
        correctUnrolledTupleIndex(index1);
        correctUnrolledTupleIndex(index2);
    }
    void trigger() final {
        mpark::visit(
            [&](auto& vToUnroll) {
                debug_code(assert(vToUnroll.size() ==
                                  op->indicesOfValuesToUnroll.size()));
                const UInt MAX_UINT = ~((UInt)0);
                UInt minIndex = MAX_UINT;
                for (size_t i = 0; i < vToUnroll.size(); i++) {
                    auto tupleFirstMember = make<IntValue>();
                    tupleFirstMember->value =
                        op->indicesOfValuesToUnroll[i] + 1;
                    auto unrolledExpr = OpMaker<OpTupleLit>::make(
                        {tupleFirstMember.asExpr(), vToUnroll[i]});
                    unrolledExpr->evaluate();
                    op->unroll(op->indicesOfValuesToUnroll[i], unrolledExpr);
                    if (op->indicesOfValuesToUnroll[i] + 1 <
                            op->unrolledIterVals.size() &&
                        op->indicesOfValuesToUnroll[i] + 1 < minIndex) {
                        minIndex = op->indicesOfValuesToUnroll[i] + 1;
                    }
                }
                vToUnroll.clear();
                op->indicesOfValuesToUnroll.clear();
                if (minIndex != MAX_UINT) {
                    correctUnrolledTupleIndices(minIndex);
                }
            },
            op->valuesToUnroll);
        deleteTrigger(op->containerDelayedTrigger);
        op->containerDelayedTrigger = nullptr;
    }
    void reattachTrigger() final {
        deleteTrigger(op->containerTrigger);
        auto trigger = make_shared<ContainerTrigger<SequenceView>>(op);
        op->container->addTrigger(trigger);
        op->containerTrigger = trigger;
    }

    void correctUnrolledTupleIndices(size_t startIndex) {
        debug_log("correct tuple indices from " << startIndex << " onwards.");
        for (size_t i = startIndex; i < op->unrolledIterVals.size(); i++) {
            correctUnrolledTupleIndex(i);
        }
    }
    void correctUnrolledTupleIndex(size_t index) {
        debug_log("Correcting tuple index " << index);
        auto& tuple =
            mpark::get<IterRef<TupleView>>(op->unrolledIterVals[index]);
        auto& intView = mpark::get<ExprRef<IntView>>(tuple->view()->members[0]);
        auto& intVal = static_cast<IntValue&>(*intView);
        intVal.changeValue([&]() {
            intVal.value = index + 1;
            return true;
        });
    }

    void hasBecomeUndefined() {
        op->containerDefined = false;
        if (op->appearsDefined()) {
            op->setAppearsDefined(false);
            visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                          op->triggers);
        }
    }
    void hasBecomeDefined() {
        this->valueChanged();
        op->containerDefined = true;
        op->setAppearsDefined(op->numberUndefined == 0);
        visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, op->triggers);
    }

    void memberHasBecomeUndefined(UInt) final {}
    void memberHasBecomeDefined(UInt) final {}
};

template <>
struct InitialUnroller<SequenceView> {
    template <typename Quant>
    static void initialUnroll(Quant& quantifier, SequenceView& containerView) {
        mpark::visit(
            [&](auto& membersImpl) {
                for (size_t i = 0; i < membersImpl.size(); i++) {
                    auto tupleFirstMember = make<IntValue>();
                    tupleFirstMember->value = i + 1;
                    auto unrolledExpr = OpMaker<OpTupleLit>::make(
                        {tupleFirstMember.asExpr(), membersImpl[i]});
                    unrolledExpr->evaluate();
                    quantifier.unroll(i, unrolledExpr);
                }
            },
            containerView.members);
    }
};

template <>
struct ContainerSanityChecker<SequenceView> {
    static void debugSanityCheck(const Quantifier<SequenceView>& quant,
                                 const SequenceView& container) {
        sanityEqualsCheck(container.numberElements(), quant.numberElements());
        sanityEqualsCheck(container.numberElements(),
                          quant.unrolledIterVals.size());
        for (size_t i = 0; i < quant.unrolledIterVals.size(); i++) {
            auto* iterPtr = mpark::get_if<IterRef<TupleView>>(
                &(quant.unrolledIterVals[i].asVariant()));
            sanityCheck(iterPtr, "Expected tuple type here.");
            auto view = (*iterPtr)->ref->view();
            sanityCheck(view, "view() should not return undefined here.");
            sanityEqualsCheck(2, view->members.size());
            auto* intExprPtr =
                mpark::get_if<ExprRef<IntView>>(&(view->members[0]));
            sanityCheck(intExprPtr,
                        "Expected first element of unrolled tuple to be int.");
            auto intView = (*intExprPtr)->getViewIfDefined();
            sanityCheck(intView,
                        "First member of unrolled tuple should be defined.");
            sanityEqualsCheck((Int)(i + 1), intView->value);
        }
    }
};

template struct Quantifier<SequenceView>;
