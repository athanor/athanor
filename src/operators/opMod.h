
#ifndef SRC_OPERATORS_OPMOD_H_
#define SRC_OPERATORS_OPMOD_H_
#include "operators/simpleOperator.h"
#include "operators/simpleTrigger.h"
#include "types/int.h"
struct OpMod;
template <>
struct OperatorTrates<OpMod> {
    typedef SimpleBinaryTrigger<OpMod, IntTrigger, true> LeftTrigger;
    typedef SimpleBinaryTrigger<OpMod, IntTrigger, false> RightTrigger;
};
struct OpMod : public SimpleBinaryOperator<IntView, IntView, OpMod> {
    using SimpleBinaryOperator<IntView, IntView, OpMod>::SimpleBinaryOperator;
};

#endif /* SRC_OPERATORS_OPMOD_H_ */
