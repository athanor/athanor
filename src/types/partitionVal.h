#ifndef SRC_TYPES_PARTITIONVAL_H_
#define SRC_TYPES_PARTITIONVAL_H_
#include <unordered_map>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/partition.h"
#include "types/set.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct PartitionDomain {
    AnyDomainRef inner;
    bool regular;
    UInt numberParts;
    UInt numberElements;
    template <typename DomainType>
    PartitionDomain(DomainType&& inner, bool regular, UInt numberParts)
        : inner(makeAnyDomainRef(std::forward<DomainType>(inner))),
          regular(regular),
          numberParts(numberParts),
          numberElements(getDomainSize(this->inner)) {
        if (numberParts == 0 || !regular) {
            std::cerr << "Error: only supporting partitions if they are "
                         "regular with a fixed number of cells.\n";
            abort();
        }
        if (numberElements % numberParts != 0) {
            std::cerr << "Error: cannot create a regular partition with this "
                         "number of parts.\n";
            prettyPrint(std::cerr, *this);
            abort();
        }
    }
    void merge(
        PartitionDomain&) { /*do nothing for now, not much supported here */
    }
};

struct PartitionValue : public PartitionView, public ValBase {
    using PartitionView::silentClear;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline ValRef<InnerValueType> member(UInt index) {
        return assumeAsValue(
            getMembers<
                typename AssociatedViewType<InnerValueType>::type>()[index]);
    }

    AnyExprVec& getChildenOperands() final;
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline bool addMember(UInt part, const ValRef<InnerValueType>& member) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        HashType hash = getValueHash(member);
        if (hashIndexMap.count(hash)) {
            return false;
        }
        hashIndexMap[hash] = memberPartMap.size();
        memberPartMap.emplace_back(part);
        getMembers<InnerViewType>().emplace_back(member.asExpr());
        if (part >= partHashes.size()) {
            size_t numberNewParts = (part + 1) - partHashes.size();
            cachedHashTotal += (numberNewParts * mix(INITIAL_HASH));

            partHashes.resize(part + 1, INITIAL_HASH);
        }
        cachedHashTotal -= mix(partHashes[part]);
        partHashes[part] += mix(hash);
        cachedHashTotal += mix(partHashes[part]);
        valBase(*member).container = this;
        valBase(*member).id = memberPartMap.size() - 1;
        return true;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool trySwapContainingParts(UInt index1, UInt index2, Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        PartitionView::swapContainingParts<InnerViewType>(index1, index2);
        if (func()) {
            auto& members = getMembers<InnerViewType>();
            valBase(*assumeAsValue(members[index1])).id = index1;
            valBase(*assumeAsValue(members[index2])).id = index2;
            PartitionView::notifyContainingPartsSwapped(index1, index2);
            debug_code(varBaseSanityChecks(*this, members));
            return true;
        } else {
            PartitionView::swapContainingParts<InnerViewType>(index1, index2);
            return false;
        }
    }

    template <typename Func>
    bool tryAssignNewValue(PartitionValue& newvalue, Func&& func) {
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
    ExprRef<PartitionView> deepCopyForUnrollImpl(
        const ExprRef<PartitionView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<PartitionView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_PARTITIONVAL_H_ */
