#ifndef SRC_TYPES_PARTITION_H_
#define SRC_TYPES_PARTITION_H_
#include <unordered_map>
#include <unordered_partition>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "triggers/partitionTrigger.h"
#include "types/set.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

static const char* NO_PARTITION_UNDEFINED_MEMBERS =
    "Not yet handling partitions with undefined members.\n";
struct PartitionDomain {
    AnyDomainRef inner;
    bool regular;
    UInt numberCells;
    UInt numberElements;
    template <typename DomainType>
    PartitionDomain(DomainType&& inner, bool regular, UInt numberCells)
        : inner(makeAnyDomainRef(std::forward<DomainType>(inner))),
          regular(regular),
          numberParts(numberParts),
          numberElements(getDomainSize(this->inner)) {
        if (numberParts == 0 || !regular) {
            std::cerr << "Error: only supporting partitions if they are "
                         "regular with a fixed number of cells.\n";
            abort();
        }
    }
};
static const char* NO_PARTITION_UNDEFINED =
    "Not currently supporting partitions with undefined members.\n";
struct PartitionView : public ExprInterface<PartitionView>,
                       public TriggerContainer<PartitionView> {
    friend PartitionValue;
    AnyExprVec members;
    std::vector<UInt> memberPartMap;
    HashType cachedHashTotal = 0;


template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    HashType getMemberHash(UInt memberIndex) {
            auto& members = getMembers<InnerViewType>();
            debug_code(assert(memberIndex < members.size()));
            return getValueHash(members[memberIndex]->getViewIfDefined().checkedGet(NO_PARTITION_UNDEFINED));
    }
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void swapContainingParts(UInt member1Index, UInt member2Index) {
        HashType member1Hash = getMemberHash<InnerViewType>(member1Index);
        HashType member2Hash = getMemberHash<InnerViewType>(member2Index);
    }

    void silentClear() {
        cachedHashTotal = 0;
        hashPartMap.clear();
        parts.clear();
    }

   public:
    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>

    inline void notifyEntirePartitionChange() {
        debug_code(assertValidState());
        visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
    }

    void assertValidState();
    void standardSanityChecksForThisType() const;
};

struct PartitionValue : public PartitionView, public ValBase {
    using PartitionView::silentClear;

    template <typename InnerViewType, EnableIfView<InnerViewType> = 0>
    void insurePartsSize(UInt partIndex) {
        while (partIndex >= parts.size()) {
            auto part = make<SetValue>();
            part.id = parts.size();
            part.container = this;
            part->members.emplace < ExprRefVec<InnerViewType>();
            parts.emplace_back(std::move(part));
        }
    }

    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline void addMember(UInt partIndex,
                          const ValRef<InnerValueType>& member) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        insurePartsSize<InnerViewType>(partIndex);

        assumeAsValue(parts[partIndex])->addMember(member);
        HashType memberHash = getValueHash(member);
        hashPartMap[memberHash] = partIndex;
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    void swapContainingParts(UInt part1Index, UInt part2Index,
                             Uint member1Index, UInt member2Index,
                             Func&& func) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        PartitionView::swapContainingParts<InnerViewType>(
            part1Index, part2Index, member1Index, member2Index);
        if (func()) {
            auto part1 = assumeAsValue(parts[part1Index]);
            auto part2 = assumeAsValue(parts[part2Index]);
            auto& base1 = valBase(
                *part1->member<InnerValueType>(part1->numberElements() - 1));
            auto& base2 = valBase(
                *part2->member<InnerValueType>(part2->numberElements() - 1));
            base1.container = &(*part1);
            base1.id = part1->numberElements() - 1;
            base2.container = &(*part2);
            base2.id = part2->numberElements() - 1;
            assertValidVarBases();
            PartitionView::notifyContainingPartsSwapped<InnerViewType>(
                part1Index, part2Index, member1Index, member2Index);
            debug_code(assertValidVarBases());
            return true;
        } else {
            PartitionView::swapContainingParts<InnerViewType>(
                part1Index, part2Index, member1Index, member2Index);
            PartitionView::notifyContainingPartsSwapped<InnerViewType>(
                part1Index, part2Index, member1Index, member2Index);
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
    ExprRef<PartitionView> deepCopyForUnrollImpl(
        const ExprRef<PartitionView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<PartitionView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_PARTITION_H_ */
