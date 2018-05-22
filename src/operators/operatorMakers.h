
#ifndef SRC_OPERATORS_OPERATORMAKERS_H_
#define SRC_OPERATORS_OPERATORMAKERS_H_
#include "base/base.h"

template <typename Op>
struct OpMaker;
struct OpIntEq;
template <>
struct OpMaker<OpIntEq> {
    static ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};
struct OpMod;
template <>
struct OpMaker<OpMod> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};
struct OpAnd;
template <>
struct OpMaker<OpAnd> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};
struct OpOr;
template <>
struct OpMaker<OpOr> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};
struct OpProd;
template <>
struct OpMaker<OpProd> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};
struct OpSequenceLit;
template <>
struct OpMaker<OpSequenceLit> {
    static ExprRef<SequenceView> make(AnyExprVec members);
};
struct OpSetNotEq;
template <>
struct OpMaker<OpSetNotEq> {
    static ExprRef<BoolView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};
struct OpSetSize;
template <>
struct OpMaker<OpSetSize> {
    static ExprRef<IntView> make(ExprRef<SetView> o);
};
struct OpSubsetEq;
template <>
struct OpMaker<OpSubsetEq> {
    static ExprRef<BoolView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};

struct OpSum;
template <>
struct OpMaker<OpSum> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};
template <typename View>
struct OpSequenceIndex;
template <typename View>
struct OpMaker<OpSequenceIndex<View>> {
    static ExprRef<View> make(ExprRef<SequenceView> sequence,
                              ExprRef<IntView> index);
};
template <typename View>
struct OpTupleIndex;

template <typename View>
struct OpMaker<OpTupleIndex<View>> {
    static ExprRef<View> make(ExprRef<TupleView> tuple, UInt index);
};

struct OpTupleLit;
template <>
struct OpMaker<OpTupleLit> {
    static ExprRef<TupleView> make(std::vector<AnyExprRef> members);
};

struct OpIsDefined;
template <>
struct OpMaker<OpIsDefined> {
    static ExprRef<BoolView> make(ExprRef<IntView> o);
};
template <typename View>
struct OpFunctionImage;

template <typename View>
struct OpMaker<OpFunctionImage<View>> {
    static ExprRef<View> make(ExprRef<FunctionView> function,
                              AnyExprRef preImage);
};

template <bool minMode>
struct OpMinMax;
typedef OpMinMax<true> OpMin;
typedef OpMinMax<false> OpMax;

template <bool minMode>
struct OpMaker<OpMinMax<minMode>> {
    static ExprRef<IntView> make(ExprRef<SequenceView>);
};
struct IntRange;
template <>
struct OpMaker<IntRange> {
    static ExprRef<SequenceView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

struct OpToInt;
template <>
struct OpMaker<OpToInt> {
    static ExprRef<IntView> make(ExprRef<BoolView> o);
};
struct OpLess;
template <>
struct OpMaker<OpLess> {
    static ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};
struct OpLessEq;
template <>
struct OpMaker<OpLessEq> {
    static ExprRef<BoolView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

#endif /* SRC_OPERATORS_OPERATORMAKERS_H_ */
