#ifndef SRC_TYPES_MSET_H_
#define SRC_TYPES_MSET_H_
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "common/common.h"
#include "types/allTypes.h"
#include "types/base.h"
#include "types/sizeAttr.h"
#include "types/typeOperations.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"

struct MSetValue;
struct MSetView;

struct MSetDomain {
    SizeAttr sizeAttr;
    AnyDomainRef inner;
    // template hack to accept only domains
    template <
        typename DomainType,
        typename std::enable_if<IsDomainType<DomainType>::value, int>::type = 0>
    MSetDomain(SizeAttr sizeAttr, DomainType&& inner)
        : sizeAttr(sizeAttr),
          inner(std::make_shared<
                typename std::remove_reference<DomainType>::type>(
              std::forward<DomainType>(inner))) {
        trimMaxSize();
    }

    // template hack to accept only pointers to domains
    template <typename DomainPtrType,
              typename std::enable_if<IsDomainPtrType<DomainPtrType>::value,
                                      int>::type = 0>
    MSetDomain(SizeAttr sizeAttr, DomainPtrType&& inner)
        : sizeAttr(sizeAttr), inner(std::forward<DomainPtrType>(inner)) {
        trimMaxSize();
    }

   private:
    inline void trimMaxSize() {
        u_int64_t innerDomainSize = getDomainSize(inner);
        if (innerDomainSize < sizeAttr.maxSize) {
            sizeAttr.maxSize = innerDomainSize;
        }
    }
};

struct MSetTrigger : public IterAssignedTrigger<MSetValue> {
    typedef MSetView View;
    virtual void valueRemoved(u_int64_t indexOfRemovedValue,
                              u_int64_t hashOfRemovedValue) = 0;
    virtual void valueAdded(const AnyValRef& member) = 0;
    virtual void possibleMemberValueChange(u_int64_t index,
                                           const AnyValRef& member) = 0;
    virtual void memberValueChanged(u_int64_t index,
                                    const AnyValRef& member) = 0;

    virtual void mSetValueChanged(const MSetView& newValue) = 0;
};
struct MSetView {
    AnyVec members;
    std::vector<std::shared_ptr<MSetTrigger>> triggers;
};

struct MSetValue : public MSetView, ValBase {};

#endif /* SRC_TYPES_MSET_H_ */
