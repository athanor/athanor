
#ifndef SRC_TYPES_ENUM_H_
#define SRC_TYPES_ENUM_H_
#include <numeric>
#include <utility>
#include <vector>

#include "base/base.h"
#include "triggers/enumTrigger.h"
#include "utils/ignoreUnused.h"

struct EnumView : public ExprInterface<EnumView>,
                  public TriggerContainer<EnumView> {
    UInt value;

    template <typename Func>
    inline bool changeValue(Func&& func) {
        UInt oldValue = value;
        if (func() && value != oldValue) {
            notifyEntireValueChanged();
            return true;
        }
        return false;
    }
    void matchValueOf(EnumView& other) {
        changeValue([&]() {
            value = other.value;
            return true;
        });
    }

    void standardSanityChecksForThisType() const;
};

#endif /* SRC_TYPES_ENUM_H_ */
