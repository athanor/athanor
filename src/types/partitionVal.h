#ifndef SRC_TYPES_PARTITIONVAL_H_
#define SRC_TYPES_PARTITIONVAL_H_
#include <unordered_map>
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "types/partition.h"
#include "types/set.h"
#include "types/sizeAttr.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct PartitionDomain {
    AnyDomainRef inner;
    bool regular;
    SizeAttr numberParts;
    SizeAttr partSize;
    UInt numberElements;
    template <typename DomainType>
    PartitionDomain(DomainType&& inner, bool regular, SizeAttr numberParts,
                    SizeAttr partSize)
        : inner(makeAnyDomainRef(std::forward<DomainType>(inner))),
          regular(regular),
          numberParts(numberParts),
          partSize(partSize),
          numberElements(getDomainSize(this->inner)) {
        fixAttributes();  // important for neighbourhood assign random
    }
    inline size_t ceilDiv(size_t l, size_t r) { return l / r + (l % r != 0); }
    inline void fixAttributes() {
        // remove 0s
        partSize.minSize = std::max<size_t>(partSize.minSize, 1);
        numberParts.minSize = std::max<size_t>(numberParts.minSize, 1);
        // remove extreme max sizes
        partSize.maxSize = std::min<size_t>(partSize.maxSize, numberElements);
        numberParts.maxSize =
            std::min<size_t>(numberParts.maxSize, numberElements);
        // just in case type is MIN_SIZE or MAX_SIZE, now that we have both mins
        // and maxes, set the types to size_range
        partSize.sizeType = SizeAttr::SizeAttrType::SIZE_RANGE;
        numberParts.sizeType = SizeAttr::SizeAttrType::SIZE_RANGE;

        // now tighten up bounds where possible
        numberParts.maxSize = std::min<size_t>(
            numberParts.maxSize, numberElements / partSize.minSize);
        partSize.maxSize = std::min<size_t>(
            partSize.maxSize,
            numberElements - ((numberParts.minSize - 1) * partSize.minSize));
        numberParts.minSize = std::max<size_t>(
            numberParts.minSize, ceilDiv(numberElements, partSize.maxSize));

        if (partSize.maxSize == partSize.minSize) {
            partSize.sizeType = SizeAttr::SizeAttrType::EXACT_SIZE;
            regular = true;
        }
        if (numberParts.minSize == numberParts.maxSize) {
            numberParts.sizeType = SizeAttr::SizeAttrType::EXACT_SIZE;
        }
    }
    inline std::shared_ptr<PartitionDomain> deepCopy(
        std::shared_ptr<PartitionDomain>&) {
        auto newDomain = std::make_shared<PartitionDomain>(*this);
        newDomain->inner = deepCopyDomain(inner);
        return newDomain;
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

    AnyExprVec& getChildrenOperands() final;

    // called only once when constructing a partition
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    void setNumberElements(size_t numberElements) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        members.emplace<ExprRefVec<InnerViewType>>(numberElements, nullptr);
        ;
        partInfo.resize(numberElements);
        memberPartMap.resize(numberElements, 0);
        emptyParts.clear();
        for (size_t i = 0; i < partInfo.size(); i++) {
            emptyParts.insert(i);
        }
    }

    // only called when constructing a partition, used to assign the first
    // member part mapping
    template <typename InnerValueType, EnableIfValue<InnerValueType> = 0>
    inline bool assignMember(UInt memberIndex, UInt part,
                             const ValRef<InnerValueType>& member) {
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        assert(memberIndex < getMembers<InnerViewType>().size());
        assert(part < partInfo.size());

        HashType hash = getValueHash(member);
        if (hashIndexMap.count(hash)) {
            return false;
        }
        hashIndexMap[hash] = memberIndex;
        memberPartMap[memberIndex] = part;
        getMembers<InnerViewType>()[memberIndex] = member.asExpr();
        cachedHashTotal -= partInfo[part].mixedPartHash();
        partInfo[part].notifyMemberAdded(member.asExpr());
        cachedHashTotal += partInfo[part].mixedPartHash();
        if (partInfo[part].partSize == 1) {
            parts.insert(part);
            emptyParts.erase(part);
        }
        valBase(*member).container = this;
        valBase(*member).id = memberIndex;
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
    // returns old parts
    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline lib::optional<std::vector<UInt>> tryMoveMembersToPart(
        const std::vector<UInt>& memberIndices, UInt destPart, Func&& func) {
        debug_code(auto partInfoBackup = partInfo);
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        const std::vector<UInt> oldParts =
            PartitionView::moveMembersToPart<InnerViewType>(memberIndices,
                                                            destPart);
        if (func()) {
            PartitionView::notifyMembersMovedToPart(memberIndices, oldParts,
                                                    destPart);
            return oldParts;
        } else {
            PartitionView::moveMembersFromPart<InnerViewType>(memberIndices,
                                                              oldParts);
            debug_code(assert(partInfoBackup == partInfo));
            return lib::nullopt;
        }
    }

    template <typename InnerValueType, typename Func,
              EnableIfValue<InnerValueType> = 0>
    inline bool tryMoveMembersFromPart(const std::vector<UInt>& memberIndices,
                                       const std::vector<UInt>& memberNewParts,
                                       Func&& func) {
        debug_code(assert(memberIndices.size() == memberNewParts.size()));
        debug_code(auto partInfoBackup = partInfo);
        typedef typename AssociatedViewType<InnerValueType>::type InnerViewType;
        UInt oldPart = memberPartMap[memberIndices[0]];
        PartitionView::moveMembersFromPart<InnerViewType>(memberIndices,
                                                          memberNewParts);
        if (func()) {
            PartitionView::notifyMembersMovedFromPart(oldPart, memberIndices,
                                                      memberNewParts);
            return true;
        } else {
            PartitionView::moveMembersToPart<InnerViewType>(memberIndices,
                                                            oldPart);
            debug_code(assert(partInfoBackup == partInfo));
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
        if (lib::get_if<ExprRefVec<InnerViewType>>(&(members)) == NULL) {
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
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<PartitionView>> optimiseImpl(
        ExprRef<PartitionView>&, PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
    void hashChecksImpl() const final;
};

#endif /* SRC_TYPES_PARTITIONVAL_H_ */
