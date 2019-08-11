#ifndef SRC_OPERATORS_definedVARHELPER_H_
#define SRC_OPERATORS_definedVARHELPER_H_
#include "base/base.h"

// lock structure, used to prevent circular walks when expressions forward
// values to variables that are defined off them.
class DefinesLock {
    static UInt64 globalStamp;
    UInt64 localStamp = 0;

   public:
    // try to acquire this lock, this will only return  true once  per round of
    // triggering.  On the next round of triggering another call will be
    // allowed.
    inline bool tryLock() {
        if (localStamp >= globalStamp) {
            return false;
        }
        localStamp = globalStamp;
        return true;
    }
    // returns whether or not try() will return true, but unlike try() the state
    // does not get changed. i.e. softTry() can be queried more than once per
    // triggering round.
    inline bool softTry() { return localStamp < globalStamp; }

    // disable this lock so that try() always returns false
    inline void disable() { localStamp = std::numeric_limits<UInt64>().max(); }

    inline bool isDisabled() const {
        return localStamp == std::numeric_limits<UInt64>().max();
    }
    // reset this lock, allow try() to return true again
    bool reset() {
        UInt64 copy = localStamp;
        localStamp = 0;
        return copy != 0;
    }

    // unlock all locks signalling the next round of triggering.  Those who
    // called tryLock() will be  able to call tryLock() again.  Those with
    // disabled locks will still not be able to call tryLock() until reset() has
    // been called on them.
    static void unlockAll() { ++globalStamp; }
};

template <typename Op>
struct DefinedVarTrigger;
void handleDefinedVarTriggers();
#endif /* SRC_OPERATORS_definedVARHELPER_H_ */
