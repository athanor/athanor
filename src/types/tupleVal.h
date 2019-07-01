#ifndef SRC_TYPES_TUPLEVAL_H_
#define SRC_TYPES_TUPLEVAL_H_
#include <vector>
#include "common/common.h"
#include "types/tuple.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

struct TupleDomain {
    std::vector<AnyDomainRef> inners;
    bool isRecord = false;
    std::vector<std::string> recordIndexNameMap;
    HashMap<std::string, size_t> recordNameIndexMap;
    template <typename... DomainTypes>
    TupleDomain(DomainTypes&&... inners)
        : inners({makeAnyDomainRef(std::forward<DomainTypes>(inners))...}) {}

    TupleDomain(std::vector<AnyDomainRef> inners) : inners(std::move(inners)) {}

    void merge(TupleDomain& other) {
        assert(inners.size() == other.inners.size());
        for (size_t i = 0; i < inners.size(); i++) {
            mergeDomains(inners[i], other.inners[i]);
        }
    }
};

struct TupleValue : public TupleView, public ValBase {
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
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

#endif /* SRC_TYPES_TUPLEVAL_H_ */
