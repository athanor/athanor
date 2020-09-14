#include "operators/opFunctionPreimage.h"

#include <iostream>
#include <memory>
#include "triggers/allTriggers.h"

#include "operators/simpleOperator.hpp"

using namespace std;
static const char* NO_PREIMAGE_UNDEFINED =
    "preimage does not yet support  undefined members.";

namespace {
HashType getHashForceDefined(const AnyExprRef& expr) {
    return lib::visit(
        [&](auto& expr) {
            auto view = expr->getViewIfDefined();
            if (!view) {
                myCerr << NO_PREIMAGE_UNDEFINED;
                myAbort();
            }
            return getValueHash(*view);
        },
        expr);
}

}  // namespace

template <typename OperandView>
void OpFunctionPreimage<OperandView>::reevaluateImpl(OperandView& image,
                                                     FunctionView& function,
                                                     bool, bool) {
    lib::visit(
        [&](auto& preimageDomain) {
            typedef typename BaseType<decltype(preimageDomain)>::element_type
                PreimageDomain;
            typedef typename AssociatedViewType<PreimageDomain>::type
                PreimageViewType;
            this->members.template emplace<ExprRefVec<PreimageViewType>>();
            this->silentClear();
            imageToPreimageHashMap.clear();
            cachedImageHash = getValueHash(image);
            for (size_t i = 0; i < function.rangeSize(); i++) {
                auto member = function.template getRange<OperandView>()[i];
                auto memberHash = getHashForceDefined(member);
                if (cachedImageHash.value() == memberHash) {
                    auto preimage = function.indexToPreimage<PreimageDomain>(i);
                    imageToPreimageHashMap[i] = getHashForceDefined(preimage);
                    this->addMember(preimage);
                }
            }
        },
        function.preimageDomain);
    this->setAppearsDefined(true);
}

template <typename OperandView>
struct OperatorTrates<OpFunctionPreimage<OperandView>>::LeftTrigger
    : public ChangeTriggerAdapter<
          typename AssociatedTriggerType<OperandView>::type,
          OperatorTrates<OpFunctionPreimage<OperandView>>::LeftTrigger> {
    OpFunctionPreimage<OperandView>* op;

   public:
    LeftTrigger(OpFunctionPreimage<OperandView>* op)
        : ChangeTriggerAdapter<
              typename AssociatedTriggerType<OperandView>::type,
              OperatorTrates<OpFunctionPreimage<OperandView>>::LeftTrigger>(
              op->left),
          op(op) {}
    ExprRef<OperandView>& getTriggeringOperand() { return op->left; }
    void adapterValueChanged() {
        op->reevaluate(true, true);
        op->notifyEntireValueChanged();
    }

    void adapterHasBecomeDefined() { todoImpl(); }

    void adapterHasBecomeUndefined() { todoImpl(); }

    void reattachTrigger() final {
        auto trigger = make_shared<
            OperatorTrates<OpFunctionPreimage<OperandView>>::LeftTrigger>(op);
        op->left->addTrigger(trigger);
        op->leftTrigger = trigger;
    }
};

template <typename OperandView>
struct OperatorTrates<OpFunctionPreimage<OperandView>>::RightTrigger
    : public FunctionTrigger {
    OpFunctionPreimage<OperandView>* op;

   public:
    RightTrigger(OpFunctionPreimage<OperandView>* op) : op(op) {}

    void imageChanged(UInt index) {
        auto& functionView =
            op->right->getViewIfDefined().checkedGet(NO_PREIMAGE_UNDEFINED);
        lib::visit(
            [&](auto& preimageDomain) {
                typedef
                    typename BaseType<decltype(preimageDomain)>::element_type
                        PreimageDomain;
                typedef typename AssociatedViewType<PreimageDomain>::type
                    PreimageView;
                auto newImageHash = getHashForceDefined(
                    functionView.template getRange<OperandView>()[index]);
                auto preimageHashIter = op->imageToPreimageHashMap.find(index);
                if (op->cachedImageHash.value() == newImageHash) {
                    auto preimage =
                        functionView.template indexToPreimage<PreimageDomain>(
                            index);
                    auto preimageHash = getHashForceDefined(preimage);
                    op->imageToPreimageHashMap[index] = preimageHash;
                    op->addMemberAndNotify(preimage);
                } else if (preimageHashIter !=
                           op->imageToPreimageHashMap.end()) {
                    op->template removeMemberAndNotify<PreimageView>(
                        op->hashIndexMap.at(preimageHashIter->second));
                    op->imageToPreimageHashMap.erase(preimageHashIter);
                }
            },
            functionView.preimageDomain);
    }
    void memberReplaced(UInt index, const AnyExprRef&) final {
        imageChanged(index);
    }
    void imageChanged(const std::vector<UInt>& indices) {
        for (auto index : indices) {
            imageChanged(index);
        }
    }
    void imageSwap(UInt index1, UInt index2) final {
        imageChanged(index1);
        imageChanged(index2);
    }
    void memberHasBecomeUndefined(UInt) final {
        myCerr << NO_PREIMAGE_UNDEFINED << endl;
        todoImpl();
    }

    void preimageChanged(UInt index, HashType) {
        auto preimageHashIter = op->imageToPreimageHashMap.find(index);
        if (preimageHashIter == op->imageToPreimageHashMap.end()) {
            return;
        }
        auto& functionView =
            op->right->getViewIfDefined().checkedGet(NO_PREIMAGE_UNDEFINED);

        lib::visit(
            [&](auto& preimageDomain) {
                typedef
                    typename BaseType<decltype(preimageDomain)>::element_type
                        PreimageDomain;
                typedef typename AssociatedViewType<PreimageDomain>::type
                    PreimageView;
                auto indexInSet = op->hashIndexMap[preimageHashIter->second];
                op->template memberChangedAndNotify<PreimageView>(indexInSet);
                preimageHashIter->second = op->indexHashMap[indexInSet];
            },
            functionView.preimageDomain);
    }
    void preimageChanged(const std::vector<UInt>& indices,
                         const std::vector<HashType>& oldHashes) {
        for (size_t i = 0; i < indices.size(); i++) {
            auto index = indices[i];
            auto hash = oldHashes[i];
            preimageChanged(index, hash);
        }
    }
    void valueAdded(const AnyExprRef&, const AnyExprRef&) {
        auto& functionView =
            op->right->getViewIfDefined().checkedGet(NO_PREIMAGE_UNDEFINED);
        imageChanged(functionView.rangeSize() - 1);
    }

    void valueRemoved(UInt index, const AnyExprRef&, const AnyExprRef&) {
        auto& functionView =
            op->right->getViewIfDefined().checkedGet(NO_PREIMAGE_UNDEFINED);
        lib::visit(
            [&](auto& preimageDomain) {
                typedef
                    typename BaseType<decltype(preimageDomain)>::element_type
                        PreimageDomain;
                typedef typename AssociatedViewType<PreimageDomain>::type
                    PreimageView;
                auto iter = op->imageToPreimageHashMap.find(index);
                if (iter != op->imageToPreimageHashMap.end()) {
                    op->template removeMemberAndNotify<PreimageView>(
                        op->hashIndexMap.at(iter->second));
                    op->imageToPreimageHashMap.erase(iter);
                }
                // keep indices in sync
                iter =
                    op->imageToPreimageHashMap.find(functionView.rangeSize());
                if (iter != op->imageToPreimageHashMap.end()) {
                    op->imageToPreimageHashMap[index] = iter->second;
                    op->imageToPreimageHashMap.erase(iter);
                }
            },
            functionView.preimageDomain);
    }

    void memberHasBecomeDefined(UInt) final {
        myCerr << NO_PREIMAGE_UNDEFINED << endl;
        todoImpl();
    }
    void valueChanged() final {
        op->reevaluate(true, true);
        op->notifyEntireValueChanged();
    }
    void hasBecomeUndefined() { todoImpl(); }
    void hasBecomeDefined() { todoImpl(); }

    void reattachTrigger() final {
        auto trigger = make_shared<
            OperatorTrates<OpFunctionPreimage<OperandView>>::RightTrigger>(op);
        op->right->addTrigger(trigger);
        op->rightTrigger = trigger;
    }
};
template <typename OperandView>
void OpFunctionPreimage<OperandView>::updateVarViolationsImpl(
    const ViolationContext& vioContext, ViolationContainer& vioContainer) {
    this->left->updateVarViolations(vioContext, vioContainer);
    this->right->updateVarViolations(vioContext, vioContainer);
}
template <typename OperandView>
void OpFunctionPreimage<OperandView>::copy(OpFunctionPreimage&) const {}
template <typename OperandView>
std::ostream& OpFunctionPreimage<OperandView>::dumpState(
    std::ostream& os) const {
    os << "OpFunctionPreimage: value=" << this->getViewIfDefined()
       << "\nLeft: ";
    this->left->dumpState(os);
    os << "\nRight: ";
    this->right->dumpState(os);
    return os;
}

template <typename OperandView>
string OpFunctionPreimage<OperandView>::getOpName() const {
    return toString(
        "OpFunctionPreimage<",
        TypeAsString<typename AssociatedValueType<OperandView>::type>::value,
        ">");
}
template <typename OperandView>
void OpFunctionPreimage<OperandView>::debugSanityCheckImpl() const {
    this->left->debugSanityCheck();
    this->right->debugSanityCheck();
    this->standardSanityChecksForThisType();
    auto leftOption = this->left->getViewIfDefined();
    auto rightOption = this->right->getViewIfDefined();
    if (!leftOption || !rightOption) {
        sanityCheck(!this->appearsDefined(), "should be undefined.");
        return;
    }
    auto& image = *leftOption;
    auto& functionView = *rightOption;

    lib::visit(
        [&](auto& preimageDomain) {
            typedef typename BaseType<decltype(preimageDomain)>::element_type
                PreimageDomain;
            sanityEqualsCheck(cachedImageHash.value(), getValueHash(image));
            HashSet<HashType> hashes;
            for (size_t i = 0; i < functionView.rangeSize(); i++) {
                auto member = functionView.template getRange<OperandView>()[i];
                if (cachedImageHash.value() == getHashForceDefined(member)) {
                    auto preimageHash = getHashForceDefined(
                        functionView.template indexToPreimage<PreimageDomain>(
                            i));
                    sanityCheck(
                        imageToPreimageHashMap.count(i),
                        toString("index ", i, " with hash ", preimageHash,
                                 " missing from imageToPreimageHashMap"));
                    sanityEqualsCheck(preimageHash,
                                      imageToPreimageHashMap.at(i));
                    hashes.insert(preimageHash);
                    sanityCheck(this->hashIndexMap.count(preimageHash),
                                toString("hash ", preimageHash, " missing."));
                }
            }
            sanityEqualsCheck(hashes.size(), this->hashIndexMap.size());
            sanityEqualsCheck(hashes.size(), imageToPreimageHashMap.size());
        },
        functionView.preimageDomain);
}

template <typename Op>
struct OpMaker;

template <typename OperandView>
struct OpMaker<OpFunctionPreimage<OperandView>> {
    static ExprRef<SetView> make(ExprRef<OperandView> l,
                                 ExprRef<FunctionView> r);
};

template <typename OperandView>
ExprRef<SetView> OpMaker<OpFunctionPreimage<OperandView>>::make(
    ExprRef<OperandView> l, ExprRef<FunctionView> r) {
    return make_shared<OpFunctionPreimage<OperandView>>(move(l), move(r));
}

#define opMakerInstantiator(name)                                            \
    template ExprRef<SetView> OpMaker<OpFunctionPreimage<name##View>>::make( \
        ExprRef<name##View>, ExprRef<FunctionView>);
buildForAllTypes(opMakerInstantiator, );
#undef opMakerInstantiator
