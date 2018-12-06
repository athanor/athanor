#ifndef SRC_TYPES_TUPLE_H_
#define SRC_TYPES_TUPLE_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "triggers/tupleTrigger.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

struct TupleDomain {
    std::vector<AnyDomainRef> inners;
    template <typename... DomainTypes>
    TupleDomain(DomainTypes&&... inners)
        : inners({makeAnyDomainRef(std::forward<DomainTypes>(inners))...}) {}

    TupleDomain(std::vector<AnyDomainRef> inners) : inners(std::move(inners)) {}
};

struct TupleView : public ExprInterface<TupleView>,
                   public TriggerContainer<TupleView> {
    friend TupleValue;
    std::vector<AnyExprRef> members;

    SimpleCache<HashType> cachedHashTotal;
    u_int32_t numberUndefined = 0;
    TupleView() {}
    TupleView(std::vector<AnyExprRef> members) : members(members) {}

    inline void memberChanged(UInt) { cachedHashTotal.invalidate(); }

   public:
    inline void memberChangedAndNotify(UInt index) {
        memberChanged(index);
        notifyMemberChanged(index);
    }

    inline void entireTupleChangeAndNotify() {
        cachedHashTotal.invalidate();
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    inline void defineMemberAndNotify(UInt index) {
        cachedHashTotal.invalidate();
        debug_code(assert(numberUndefined > 0));
        numberUndefined--;
        if (numberUndefined == 0) {
            this->setAppearsDefined(true);
        }
        notifyMemberDefined(index);
    }

    inline void undefineMemberAndNotify(UInt index) {
        cachedHashTotal.invalidate();
        numberUndefined++;
        this->setAppearsDefined(false);
        notifyMemberUndefined(index);
    }
    void standardSanityChecksForThisType() const;
};

struct TupleValue : public TupleView, public ValBase {
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> value(UInt index) {
        return assumeAsValue(
            mpark::get<
                ExprRef<typename AssociatedViewType<InnerValueType>::type>>(
                members[index]));
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void addMember(const ValRef<InnerValueType>& member) {
        cachedHashTotal.invalidate();
        members.emplace_back(member.asExpr());
        valBase(*member).container = this;
        valBase(*member).id = members.size() - 1;
        debug_code(assertValidVarBases());
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMemberChange(size_t index, Func&& func) {
        if (func()) {
            TupleView::memberChanged(index);
            TupleView::notifyMemberChanged(index);
            return true;
        } else {
            return false;
        }
    }

    template <typename Func>
    bool tryAssignNewValue(TupleValue& newvalue, Func&& func) {
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
    void printVarBases();
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<TupleView> deepCopyForUnrollImpl(
        const ExprRef<TupleView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<TupleView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_TUPLE_H_ */
