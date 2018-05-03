#ifndef SRC_TYPES_TUPLE_H_
#define SRC_TYPES_TUPLE_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
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
struct TupleView;
struct TupleTrigger : public virtual TriggerBase {
    virtual void possibleMemberValueChange(UInt index) = 0;
    virtual void memberValueChanged(UInt index) = 0;
};
struct TupleValue;
struct TupleView : public ExprInterface<TupleView> {
    friend TupleValue;
    std::vector<AnyExprRef> members;
    std::vector<std::shared_ptr<TupleTrigger>> triggers;
    SimpleCache<HashType> cachedHashTotal;

    TupleView() {}
    TupleView(std::vector<AnyExprRef> members) : members(members) {}

   private:
    inline void memberChanged(UInt) { cachedHashTotal.invalidate(); }

    inline void notifyMemberChanged(UInt index) {
        visitTriggers([&](auto& t) { t->memberValueChanged(index); }, triggers);
    }

   public:
    inline void notifyPossibleMemberChange(UInt index) {
        visitTriggers([&](auto& t) { t->possibleMemberValueChange(index); },
                      triggers);
    }
    inline void notifyPossibleEntireTupleChange() {
        visitTriggers([&](auto& t) { t->possibleValueChange(); }, triggers);
    }

    inline void memberChangedAndNotify(UInt index) {
        memberChanged(index);
        notifyMemberChanged(index);
    }

    inline void entireTupleChangeAndNotify() {
        cachedHashTotal.invalidate();
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    inline void initFrom(TupleView&) {
        std::cerr << "not allowed\n" << __func__ << std::endl;
    }
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
        TupleView::notifyPossibleMemberChange(index);

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
    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<TupleView> deepCopySelfForUnroll(
        const ExprRef<TupleView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    bool isUndefined();
};

template <typename Child>
struct ChangeTriggerAdapter<TupleTrigger, Child>
    : public TupleTrigger, public ChangeTriggerAdapterBase<Child> {
    inline void possibleMemberValueChange(UInt) final {
        this->forwardPossibleValueChange();
    }
    inline void memberValueChanged(UInt) final { this->forwardValueChanged(); }
    inline void possibleValueChange() final {
        this->forwardPossibleValueChange();
    }
    inline void valueChanged() final { this->forwardValueChanged(); }
};

#endif /* SRC_TYPES_TUPLE_H_ */
