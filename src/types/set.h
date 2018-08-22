#ifndef SRC_TYPES_SET_H_
#define SRC_TYPES_SET_H_
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct SetDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    template <typename DomainType>
    SetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(makeAnyDomainRef(std::forward<DomainType>(inner))) {
        trimMaxSize();
    }

   private:
    inline void trimMaxSize() {
        UInt innerDomainSize = getDomainSize(inner);
        if (innerDomainSize < sizeAttr.maxSize) {
            sizeAttr.maxSize = innerDomainSize;
        }
        if (sizeAttr.maxSize < sizeAttr.minSize) {
            std::cerr << "Error, either by deduction or from original "
                         "specification, deduced max size of set to be less "
                         "than min size.\n"
                      << *this << std::endl;
            abort();
        }
    }
};

struct SetTrigger : public virtual TriggerBase {
    virtual void valueRemoved(UInt indexOfRemovedValue,
                              HashType hashOfRemovedValue) = 0;
    virtual void valueAdded(const AnyExprRef& member) = 0;
    virtual void possibleMemberValueChange(UInt index) = 0;
    virtual void memberValueChanged(UInt index) = 0;
    virtual void possibleMemberValuesChange(
        const std::vector<UInt>& indices) = 0;
    virtual void memberValuesChanged(const std::vector<UInt>& indices) = 0;
};
struct SetView : public ExprInterface<SetView> {
    friend SetValue;
    std::unordered_set<HashType> memberHashes;
    AnyExprVec members;
    HashType cachedHashTotal = 0;
    std::vector<std::shared_ptr<SetTrigger>> triggers;
    debug_code(bool posSetValueChangeCalled = false);

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool addMember(const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        if (containsMember(member)) {
            return false;
        }

        HashType hash = getValueHash(member->view());
        debug_code(assert(!memberHashes.count(hash)));
        members.emplace_back(member);
        memberHashes.insert(hash);
        cachedHashTotal += mix(hash);
        debug_code(assertValidState());
        return true;
    }

    inline void notifyMemberAdded(const AnyExprRef& newMember) {
        debug_code(assert(posSetValueChangeCalled);
                   posSetValueChangeCalled = false);
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueAdded(newMember); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));

        HashType hash = getValueHash(members[index]->view());
        debug_code(assert(memberHashes.count(hash)));
        memberHashes.erase(hash);
        cachedHashTotal -= mix(hash);
        auto removedMember = std::move(members[index]);
        members[index] = std::move(members.back());
        members.pop_back();
        return removedMember;
    }

    inline void notifyMemberRemoved(UInt index, HashType hashOfRemovedMember) {
        debug_code(assert(posSetValueChangeCalled);
                   posSetValueChangeCalled = false);
        debug_code(assertValidState());
        visitTriggers(
            [&](auto& t) { t->valueRemoved(index, hashOfRemovedMember); },
            triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType memberChanged(HashType oldHash, UInt index) {
        auto& members = getMembers<InnerViewType>();
        HashType newHash = getValueHash(members[index]->view());
        if (newHash != oldHash) {
            debug_code(assert(!memberHashes.count(newHash)));
            memberHashes.erase(oldHash);
            memberHashes.insert(newHash);
            cachedHashTotal -= mix(oldHash);
            cachedHashTotal += mix(newHash);
        }
        debug_code(assertValidState());
        return newHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void membersChanged(std::vector<HashType>& hashes,
                               const std::vector<UInt>& indices) {
        auto& members = getMembers<InnerViewType>();
        for (HashType oldHash : hashes) {
            cachedHashTotal -= mix(oldHash);
            bool erased = memberHashes.erase(oldHash);
            ignoreUnused(erased);
            debug_code(assert(erased));
        }
        std::transform(
            indices.begin(), indices.end(), hashes.begin(),
            [&](UInt index) { return getValueHash(members[index]->view()); });
        for (HashType newHash : hashes) {
            cachedHashTotal += mix(newHash);
            debug_code(assert(!memberHashes.count(newHash)));
            memberHashes.insert(newHash);
        }
        debug_code(assertValidState());
    }

    inline void notifyMemberChanged(size_t index) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->memberValueChanged(index); }, triggers);
    }
    inline void notifyMembersChanged(const std::vector<UInt>& indices) {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->memberValuesChanged(indices); },
                      triggers);
    }

    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal = 0;
                memberHashes.clear();
                membersImpl.clear();

            },
            members);
    }

   public:
    inline void notifyPossibleSetValueChange() {
        visitTriggers([](auto& t) { t->possibleValueChange(); }, triggers);
        debug_code(posSetValueChangeCalled = true);
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool addMemberAndNotify(const ExprRef<InnerViewType>& member) {
        notifyPossibleSetValueChange();
        if (addMember(member)) {
            notifyMemberAdded(getMembers<InnerViewType>().back());
            return true;
        } else {
            return false;
        }
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMemberAndNotify(UInt index) {
        notifyPossibleSetValueChange();
        ExprRef<InnerViewType> removedValue =
            removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, getValueHash(*removedValue));
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
    template <typename InnerViewType>
    inline void notifyPossibleMembersChange(const std::vector<UInt>& indices,
                                            std::vector<HashType>& hashes) {
        auto& members = getMembers<InnerViewType>();
        hashes.resize(indices.size());
        std::transform(
            indices.begin(), indices.end(), hashes.begin(),
            [&](UInt index) { return getValueHash(members[index]->view()); });
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->possibleMemberValuesChange(indices); },
                      triggers);
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

    inline void notifyEntireSetChange() {
        debug_code(assert(posSetValueChangeCalled);
                   posSetValueChangeCalled = false);

        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRefVec<InnerViewType>& getMembers() {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline const ExprRefVec<InnerViewType>& getMembers() const {
        return mpark::get<ExprRefVec<InnerViewType>>(members);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool containsMember(const ExprRef<InnerViewType>& member) const {
        return containsMember(member->view());
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline bool containsMember(const InnerViewType& member) const {
        return memberHashes.count(getValueHash(member));
    }

    inline UInt numberElements() const { return memberHashes.size(); }
    void assertValidState();
};

struct SetValue : public SetView, public ValBase {
    using SetView::silentClear;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getMembers<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline bool addMember(const ValRef<InnerValueType>& member) {
        if (SetView::addMember(member.asExpr())) {
            valBase(*member).container = this;
            valBase(*member).id = numberElements() - 1;
            debug_code(assertValidVarBases());
            return true;
        } else {
            return false;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryAddMember(const ValRef<InnerValueType>& member,
                             Func&& func) {
        notifyPossibleSetValueChange();
        if (SetView::addMember(member.asExpr())) {
            if (func()) {
                valBase(*member).container = this;
                valBase(*member).id = numberElements() - 1;
                SetView::notifyMemberAdded(member.asExpr());
                debug_code(assertValidVarBases());
                return true;
            } else {
                typedef typename AssociatedViewType<InnerValueType>::type
                    InnerViewType;
                SetView::removeMember<InnerViewType>(numberElements() - 1);
            }
        }
        return false;
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> removeMember(UInt index) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember =
            assumeAsValue(SetView::removeMember<InnerViewType>(index));
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
        notifyPossibleSetValueChange();
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        auto removedMember =
            assumeAsValue(SetView::removeMember<InnerViewType>(index));
        if (func()) {
            valBase(*removedMember).container = NULL;
            if (index < numberElements()) {
                valBase(*member<InnerValueType>(index)).id = index;
            }
            debug_code(assertValidVarBases());
            SetView::notifyMemberRemoved(index,
                                         getValueHash(asView(*removedMember)));
            return std::make_pair(true, std::move(removedMember));
        } else {
            SetView::addMember<InnerViewType>(removedMember.asExpr());
            auto& members = getMembers<InnerViewType>();
            std::swap(members[index], members.back());
            memberHashes.insert(getValueHash(members[index]->view()));
            debug_code(assertValidState());
            debug_code(assertValidVarBases());
            return std::make_pair(false, ValRef<InnerValueType>(nullptr));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline HashType notifyPossibleMemberChange(UInt index) {
        return SetView::notifyPossibleMemberChange<
            typename AssociatedViewType<InnerValueType>::type>(index);
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void notifyPossibleMembersChange(const std::vector<UInt>& indices,
                                            std::vector<HashType>& hashes) {
        SetView::notifyPossibleMembersChange<
            typename AssociatedViewType<InnerValueType>::type>(indices, hashes);
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline std::pair<bool, HashType> tryMemberChange(size_t index,
                                                     HashType oldHash,
                                                     Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        HashType newHash = memberChanged<InnerViewType>(oldHash, index);
        if (func()) {
            SetView::notifyMemberChanged(index);
            return std::make_pair(true, newHash);
        } else {
            if (oldHash != newHash) {
                memberHashes.insert(oldHash);
                memberHashes.erase(newHash);
                cachedHashTotal -= mix(newHash);
                cachedHashTotal += mix(oldHash);
            }
            return std::make_pair(false, oldHash);
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMembersChange(const std::vector<UInt>& indices,
                                 std::vector<HashType>& hashes, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        std::vector<HashType> newHashes(hashes.begin(), hashes.end());
        membersChanged<InnerViewType>(newHashes, indices);
        HashType cachedHashTotalBackup = cachedHashTotal;
        if (func()) {
            SetView::notifyMembersChanged(indices);
            return true;
        } else {
            cachedHashTotal = cachedHashTotalBackup;
            for (auto hash : newHashes) {
                memberHashes.erase(hash);
            }
            for (auto hash : hashes) {
                memberHashes.insert(hash);
            }
            debug_code(assertValidState());
            return false;
        }
    }

    template <typename Func>
    bool tryAssignNewValue(SetValue& newvalue, Func&& func) {
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
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SetView> deepCopySelfForUnrollImpl(
        const ExprRef<SetView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
    std::pair<bool, ExprRef<SetView>> optimise(PathExtension) final;
};

template <typename Child>
struct ChangeTriggerAdapter<SetTrigger, Child>
    : public ChangeTriggerAdapterBase<SetTrigger, Child> {
    inline void valueRemoved(UInt, HashType) { this->forwardValueChanged(); }
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

#endif /* SRC_TYPES_SET_H_ */
