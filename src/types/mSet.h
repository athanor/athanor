#ifndef SRC_TYPES_MSET_H_
#define SRC_TYPES_MSET_H_
#include <unordered_map>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct MSetDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    template <typename DomainType>
    MSetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(makeAnyDomainRef(std::forward<DomainType>(inner))) {
        checkMaxSize();
    }

   private:
    void checkMaxSize() {
        if (sizeAttr.sizeType == SizeAttr::SizeAttrType::NO_SIZE ||
            sizeAttr.sizeType == SizeAttr::SizeAttrType::MIN_SIZE) {
            std::cerr << "Error, MSet domain must be initialised with "
                         "maxSize() or exactSize()\n";
            abort();
        }
    }
};

struct MSetTrigger : public virtual TriggerBase {
    virtual void valueRemoved(UInt indexOfRemovedValue,
                              const AnyExprRef& removedExpr) = 0;
    virtual void valueAdded(const AnyExprRef& member) = 0;
    virtual void possibleMemberValueChange(UInt index) = 0;
    virtual void memberValueChanged(UInt index) = 0;
    virtual void possibleMemberValuesChange(
        const std::vector<UInt>& indices) = 0;
    virtual void memberValuesChanged(const std::vector<UInt>& indices) = 0;
};

struct MSetView : public ExprInterface<MSetView> {
    friend MSetValue;
    AnyExprVec members;
    HashType cachedHashTotal = 0;
    std::vector<std::shared_ptr<MSetTrigger>> triggers;
    debug_code(bool posMSetValueChangeCalled = false);

   private:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        HashType hash = getValueHash(member->view());
        members.emplace_back(member);
        cachedHashTotal += mix(hash);
        debug_code(assertValidState());
    }

    inline void notifyMemberAdded(const AnyExprRef& newMember) {
        debug_code(assert(posMSetValueChangeCalled);
                   posMSetValueChangeCalled = false);
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));

        HashType hash = getValueHash(members[index]->view());
        cachedHashTotal -= mix(hash);
        ExprRef<InnerViewType> removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        return removedMember;
    }

    inline void notifyMemberRemoved(UInt index, const AnyExprRef& expr) {
        debug_code(assert(posMSetValueChangeCalled);
                   posMSetValueChangeCalled = false);
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueRemoved(index, expr); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline UInt memberChanged(HashType oldHash, UInt index) {
        auto& members = getMembers<InnerViewType>();
        HashType newHash = getValueHash(members[index]->view());
        if (newHash != oldHash) {
            cachedHashTotal -= mix(oldHash);
            cachedHashTotal += mix(newHash);
        }
        debug_code(assertValidState());
        return newHash;
    }

    inline void notifyMemberChanged(size_t index) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->memberValueChanged(index); }, triggers);
    }

    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal = 0;
                membersImpl.clear();
            },
            members);
    }

   public:
    inline void notifyPossibleMSetValueChange() {
        visitTriggers([](auto& t) { t->possibleValueChange(); }, triggers);
        debug_code(posMSetValueChangeCalled = true);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMemberAndNotify(const ExprRef<InnerViewType>& member) {
        notifyPossibleMSetValueChange();
        addMember(member);
        notifyMemberAdded(getMembers<InnerViewType>().back());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMemberAndNotify(UInt index) {
        notifyPossibleMSetValueChange();
        ExprRef<InnerViewType> removedValue =
            removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, removedValue);
        return removedValue;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType notifyPossibleMemberChange(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assertValidState());
        HashType memberHash = getValueHash(members[index]->view());
        visitTriggers([&](auto& t) { t->possibleMemberValueChange(index); },
                      triggers);
        return memberHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChangedAndNotify(size_t index, HashType oldHash) {
        HashType newHash = memberChanged<InnerViewType>(oldHash, index);
        if (oldHash == newHash) {
            return newHash;
        }
        notifyMemberChanged(index);
        return newHash;
    }

    inline void notifyEntireMSetChange() {
        debug_code(assert(posMSetValueChangeCalled);
                   posMSetValueChangeCalled = false);
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    inline void initFrom(MSetView& other) {
        members = other.members;
        cachedHashTotal = other.cachedHashTotal;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    inline UInt numberElements() const {
        return mpark::visit([](auto& members) { return members.size(); },
                            members);
    }
    void assertValidState();
};

struct MSetValue : public MSetView, public ValBase {
    using MSetView::silentClear;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getMembers<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void addMember(const ValRef<InnerValueType>& member) {
        MSetView::addMember(member.asExpr());
        valBase(*member).container = this;
        valBase(*member).id = numberElements() - 1;
        debug_code(assertValidVarBases());
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(const ValRef<InnerValueType>& member,
                             Func&& func) {
        notifyPossibleMSetValueChange();
        MSetView::addMember(member.asExpr());
        if (func()) {
            valBase(*member).container = this;
            valBase(*member).id = numberElements() - 1;
            MSetView::notifyMemberAdded(member.asExpr());
            debug_code(assertValidVarBases());
            return true;
        } else {
            typedef
                typename AssociatedViewType<InnerValueType>::type InnerViewType;
            MSetView::removeMember<InnerViewType>(numberElements() - 1);
            valBase(*member).container = NULL;
            return false;
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> removeMember(UInt index) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember =
            assumeAsValue(MSetView::removeMember<InnerViewType>(index));
        valBase(*removedMember).container = NULL;
        if (index < numberElements()) {
            valBase(*member<InnerValueType>(index)).id = index;
        }
        debug_code(assertValidVarBases());
        return removedMember;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, ValRef<InnerValueType>> tryRemoveMember(
        UInt index, Func&& func) {
        notifyPossibleMSetValueChange();
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember =
            assumeAsValue(MSetView::removeMember<InnerViewType>(index));
        if (func()) {
            valBase(*removedMember).container = NULL;
            if (index < numberElements()) {
                valBase(*member<InnerValueType>(index)).id = index;
            }
            debug_code(assertValidVarBases());
            MSetView::notifyMemberRemoved(index, removedMember.asExpr());
            return std::make_pair(true, std::move(removedMember));
        } else {
            MSetView::addMember<InnerViewType>(removedMember.asExpr());
            auto& members = getMembers<InnerViewType>();
            std::swap(members[index], members.back());
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline HashType notifyPossibleMemberChange(UInt index) {
        return MSetView::notifyPossibleMemberChange<
            typename AssociatedViewType<InnerValueType>::type>(index);
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, HashType> tryMemberChange(size_t index,
                                                     HashType oldHash,
                                                     Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        HashType newHash = memberChanged<InnerViewType>(oldHash, index);
        if (func()) {
            MSetView::notifyMemberChanged(index);
            return std::make_pair(true, newHash);
        } else {
            if (oldHash != newHash) {
                cachedHashTotal -= mix(newHash);
                cachedHashTotal += mix(oldHash);
            }
            return std::make_pair(false, oldHash);
        }
    }

    template <typename Func>
    bool tryAssignNewValue(MSetValue& newvalue, Func&& func) {
        // fake putting in the value first untill func()verifies that it is
        // happy with the change
        std::swap(*this, newvalue);
        bool allowed = func();
        std::swap(*this, newvalue);
        if (allowed) {
            deepCopy(newvalue, *this);
        }
        return allowed;
    }
    void assertValidVarBases();

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void setInnerType() {
        if (mpark::get_if<ExprRefVec<InnerViewType>>(&(members)) == NULL) {
            members.emplace<ExprRefVec<InnerViewType>>();
        }
    }

    void printVarBases();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolations(const ViolationContext& vioContext,
                             ViolationContainer&) final;
    ExprRef<MSetView> deepCopySelfForUnrollImpl(
        const ExprRef<MSetView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    std::pair<bool, ExprRef<MSetView>> optimise(PathExtension) final;
};

template <typename Child>
struct ChangeTriggerAdapter<MSetTrigger, Child>
    : public ChangeTriggerAdapterBase<MSetTrigger, Child> {
    inline void valueRemoved(UInt, const AnyExprRef&) {
        this->forwardValueChanged();
    }
    inline void valueAdded(const AnyExprRef&) final {
        this->forwardValueChanged();
    }
    inline void possibleMemberValueChange(UInt) final {
        this->forwardPossibleValueChange();
    }
    inline void memberValueChanged(UInt) final { this->forwardValueChanged(); }
    inline void possibleMemberValuesChange(const std::vector<UInt>&) final {
        this->forwardPossibleValueChange();
    }
    inline void memberValuesChanged(const std::vector<UInt>&) final {
        this->forwardValueChanged();
    }
};

#endif /* SRC_TYPES_MSET_H_ */
