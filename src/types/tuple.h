#ifndef SRC_TYPES_TUPLE_H_
#define SRC_TYPES_TUPLE_H_
#include <vector>
#include "base/base.h"
#include "common/common.h"
#include "triggers/tupleTrigger.h"
#include "utils/hashUtils.h"
#include "utils/ignoreUnused.h"
#include "utils/simpleCache.h"

struct TupleView : public ExprInterface<TupleView>,
                   public TriggerContainer<TupleView> {
    friend TupleValue;
    std::vector<AnyExprRef> members;

    SimpleCache<HashType> cachedHashTotal;
    UInt32 numberUndefined = 0;
    TupleView() {}
    TupleView(std::vector<AnyExprRef> members) : members(members) {}

    inline void memberChanged(UInt) { cachedHashTotal.invalidate(); }
    inline void memberReplaced(UInt, const AnyExprRef&) {
        cachedHashTotal.invalidate();
    }

   public:
    inline void memberChangedAndNotify(UInt index) {
        memberChanged(index);
        notifyMemberChanged(index);
    }

    inline void memberReplacedAndNotify(UInt index, const AnyExprRef& expr) {
        memberReplaced(index, expr);
        notifyMemberReplaced(index, expr);
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

    void silentClear() {
        members.clear();
        cachedHashTotal.invalidate();
        numberUndefined = 0;
    }
};

#endif /* SRC_TYPES_TUPLE_H_ */
