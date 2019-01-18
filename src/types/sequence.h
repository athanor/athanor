#ifndef SRC_TYPES_SEQUENCE_H_
#define SRC_TYPES_SEQUENCE_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "triggers/sequenceTrigger.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

// sequence calls getValueHash on ExprRef<T> so neeed to forward declare the
// specialisation
template <>
HashType getValueHash<SequenceView>(const SequenceView& val);
;

static const char* NO_SEQUENCE_HASHING_UNDEFINED =
    "sequence is not correctly handling hashing values that appear "
    "defined but return undefined views.\n";

struct SequenceView : public ExprInterface<SequenceView>,
                      public TriggerContainer<SequenceView> {
    friend SequenceValue;
    AnyExprVec members;
    SimpleCache<HashType> cachedHashTotal;
    UInt numberUndefined = 0;

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType calcMemberHash(UInt index,
                                   const ExprRef<InnerViewType>& expr) const {
        HashType input[2];
        input[0] = HashType(index);
        input[1] = getValueHash(
            expr->view().checkedGet(NO_SEQUENCE_HASHING_UNDEFINED));
        return mix(((char*)input), sizeof(input));
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType calcSubsequenceHash(UInt start, UInt end) const {
        HashType total = HashType(0);
        for (size_t i = start; i < end; ++i) {
            total += calcMemberHash(i, getMembers<InnerViewType>()[i]);
        }
        return total;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMember(size_t index, const ExprRef<InnerViewType>& member) {
        auto& members = getMembers<InnerViewType>();
        members.insert(members.begin() + index, member);
        bool memberUndefined = !member->appearsDefined();
        if (!memberUndefined && index == members.size() - 1) {
            cachedHashTotal.applyIfValid([&](auto& value) {
                value += this->calcMemberHash(index, member);
            });
        } else {
            cachedHashTotal.invalidate();
        }
        if (memberUndefined) {
            numberUndefined++;
            this->setAppearsDefined(false);
        }
        debug_code(assertValidState());
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMember(UInt index) {
        auto& members = getMembers<InnerViewType>();
        debug_code(assert(index < members.size()));
        ExprRef<InnerViewType> removedMember = std::move(members[index]);
        members.erase(members.begin() + index);
        if (index == members.size()) {
            cachedHashTotal.applyIfValid([&](auto& value) {
                value -= this->calcMemberHash(index, removedMember);
            });
        } else {
            cachedHashTotal.invalidate();
        }
        if (!removedMember->appearsDefined()) {
            numberUndefined--;
            if (numberUndefined == 0) {
                this->setAppearsDefined(true);
            }
        }
        debug_code(assertValidState());
        return removedMember;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void swapPositions(UInt index1, UInt index2) {
        auto& members = getMembers<InnerViewType>();
        std::swap(members[index1], members[index2]);
        cachedHashTotal.applyIfValid([&](auto& value) {
            value -= this->calcMemberHash(index1, members[index2]);
            value -= this->calcMemberHash(index2, members[index1]);
            value += this->calcMemberHash(index1, members[index1]);
            value += this->calcMemberHash(index2, members[index2]);
        });
        debug_code(assertValidState());
    }

    inline void checkNotUsingCachedHash() {
        if (cachedHashTotal.isValid()) {
            std::cerr
                << "Error: constraint changing a subsequence without "
                   "passing a previous subsequence hash.  Means no support for "
                   "getting total hash of this sequence.  Suspected "
                   "reason is that one of the constraints has not been "
                   "updated to support this.\n";
            abort();
        }
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType changeSubsequence(UInt startIndex, UInt endIndex) {
        checkNotUsingCachedHash();
        return changeSubsequence<InnerViewType>(startIndex, endIndex,
                                                HashType(0));
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType changeSubsequence(UInt startIndex, UInt endIndex,
                                      HashType previousSubsequenceHash) {
        HashType newHash = HashType(0);
        cachedHashTotal.applyIfValid([&](auto& value) {
            newHash =
                this->calcSubsequenceHash<InnerViewType>(startIndex, endIndex);
            value -= previousSubsequenceHash;
            value += newHash;
        });
        return newHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void swapAndNotify(UInt index1, UInt index2) {
        swapPositions<InnerViewType>(index1, index2);
        notifyPositionsSwapped(index1, index2);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void defineMemberAndNotify(UInt index) {
        cachedHashTotal.applyIfValid([&](auto& value) {
            value +=
                this->calcMemberHash(index, getMembers<InnerViewType>()[index]);
        });
        debug_code(assert(numberUndefined > 0));
        numberUndefined--;
        if (numberUndefined == 0) {
            this->setAppearsDefined(true);
        }
        notifyMemberDefined(index);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void undefineMemberAndNotify(UInt index) {
        checkNotUsingCachedHash();
        undefineMemberAndNotify<InnerViewType>(index, HashType(0));
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void undefineMemberAndNotify(UInt index,
                                        HashType hashOfPossibleChange) {
        cachedHashTotal.applyIfValid(
            [&](auto& value) { value -= hashOfPossibleChange; });
        numberUndefined++;
        this->setAppearsDefined(false);
        notifyMemberUndefined(index);
    }

    void silentClear() {
        mpark::visit(
            [&](auto& membersImpl) {
                cachedHashTotal.invalidate();
                membersImpl.clear();
            },
            members);
        numberUndefined = 0;
    }

   public:
    SequenceView() {}
    SequenceView(AnyExprVec members) : members(std::move(members)) {}
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void addMemberAndNotify(UInt index,
                                   const ExprRef<InnerViewType>& member) {
        addMember(index, member);
        notifyMemberAdded(index, getMembers<InnerViewType>()[index]);
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline ExprRef<InnerViewType> removeMemberAndNotify(UInt index) {
        ExprRef<InnerViewType> removedValue =
            removeMember<InnerViewType>(index);
        notifyMemberRemoved(index, removedValue);
        return removedValue;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline HashType notifyPossibleSubsequenceChange(UInt startIndex,
                                                    UInt endIndex) {
        debug_code(assertValidState());
        HashType previousSubSequenceHash = HashType(0);
        if (cachedHashTotal.isValid()) {
            previousSubSequenceHash =
                calcSubsequenceHash<InnerViewType>(startIndex, endIndex);
        }
        return previousSubSequenceHash;
    }

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    inline void changeSubsequenceAndNotify(UInt startIndex, UInt endIndex) {
        changeSubsequence<InnerViewType>(startIndex, endIndex);
        notifySubsequenceChanged(startIndex, endIndex);
    }

    inline void notifyEntireSequenceChange() {
        cachedHashTotal.invalidate();
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }
    inline void initFrom(SequenceView&) {
        std::cerr << "Deprecated, Should never be called\n"
                  << __func__ << std::endl;
        abort();
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

    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_SEQUENCE_H_ */
