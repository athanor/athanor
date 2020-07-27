
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

struct OpBoolEq;
template <>
struct OpMaker<OpBoolEq> {
    static ExprRef<BoolView> make(ExprRef<BoolView> l, ExprRef<BoolView> r);
};

struct OpEnumEq;
template <>
struct OpMaker<OpEnumEq> {
    static ExprRef<BoolView> make(ExprRef<EnumView> l, ExprRef<EnumView> r);
};

struct OpMod;
template <>
struct OpMaker<OpMod> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

struct OpNegate;
template <>
struct OpMaker<OpNegate> {
    static ExprRef<IntView> make(ExprRef<IntView> o);
};

struct OpNot;
template <>
struct OpMaker<OpNot> {
    static ExprRef<BoolView> make(ExprRef<BoolView> o);
};

struct OpAnd;
template <>
struct OpMaker<OpAnd> {
    static ExprRef<BoolView> make(
        ExprRef<SequenceView>,
        const std::shared_ptr<SequenceDomain>& operandDomain = nullptr);
};
struct OpOr;
template <>
struct OpMaker<OpOr> {
    static ExprRef<BoolView> make(
        ExprRef<SequenceView>,
        const std::shared_ptr<SequenceDomain>& operandDomain = nullptr);
};
struct OpProd;
template <>
struct OpMaker<OpProd> {
    static ExprRef<IntView> make(
        ExprRef<SequenceView>,
        const std::shared_ptr<SequenceDomain>& operandDomain = nullptr);
};
struct OpSequenceLit;
template <>
struct OpMaker<OpSequenceLit> {
    static ExprRef<SequenceView> make(AnyExprVec members);
};

struct OpMSetLit;
template <>
struct OpMaker<OpMSetLit> {
    static ExprRef<MSetView> make(AnyExprVec members);
};

template <typename OperandView>
struct OpNotEq;
template <typename OperandView>
struct OpMaker<OpNotEq<OperandView>> {
    static ExprRef<BoolView> make(ExprRef<OperandView> l,
                                  ExprRef<OperandView> r);
};
struct OpSetSize;
template <>
struct OpMaker<OpSetSize> {
    static ExprRef<IntView> make(ExprRef<SetView> o);
};
struct OpMSetSize;
template <>
struct OpMaker<OpMSetSize> {
    static ExprRef<IntView> make(ExprRef<MSetView> o);
};

struct OpSequenceSize;
template <>
struct OpMaker<OpSequenceSize> {
    static ExprRef<IntView> make(ExprRef<SequenceView> o);
};

struct OpSubsetEq;
template <>
struct OpMaker<OpSubsetEq> {
    static ExprRef<BoolView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};

struct OpSum;
template <>
struct OpMaker<OpSum> {
    static ExprRef<IntView> make(
        ExprRef<SequenceView>,
        const std::shared_ptr<SequenceDomain>& operandDomain = nullptr);
};
template <typename View>
struct OpSequenceIndex;
template <typename View>
struct OpMaker<OpSequenceIndex<View>> {
    static ExprRef<View> make(ExprRef<SequenceView> sequence,
                              ExprRef<IntView> index);
};
template <typename View>
struct OpUndefined;
template <typename View>
struct OpMaker<OpUndefined<View>> {
    static ExprRef<View> make();
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
template <typename View>
struct OpIsDefined;
template <typename View>
struct OpMaker<OpIsDefined<View>> {
    static ExprRef<BoolView> make(ExprRef<View> o);
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
    static ExprRef<IntView> make(
        ExprRef<SequenceView>,
        const std::shared_ptr<SequenceDomain>& operandDomain = nullptr);
};
struct IntRange;
template <>
struct OpMaker<IntRange> {
    static ExprRef<SequenceView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

struct EnumRange;
template <>
struct OpMaker<EnumRange> {
    static ExprRef<SequenceView> make(std::shared_ptr<EnumDomain> domain);
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

struct OpMinus;
template <>
struct OpMaker<OpMinus> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

template <typename OperandView>
struct OpSetLit;
template <>
struct OpMaker<OpSetLit<FunctionView>> {
    static ExprRef<SetView> make(ExprRef<FunctionView> o);
};

struct OpPowerSet;
template <>
struct OpMaker<OpPowerSet> {
    static ExprRef<SetView> make(ExprRef<SetView> o);
};

struct OpPower;
template <>
struct OpMaker<OpPower> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};
struct OpIn;
template <>
struct OpMaker<OpIn> {
    static ExprRef<BoolView> make(AnyExprRef expr, ExprRef<SetView> setOperand);
};
struct OpImplies;
template <>
struct OpMaker<OpImplies> {
    static ExprRef<BoolView> make(ExprRef<BoolView> l, ExprRef<BoolView> r);
};
template <typename View>
struct OpSetIndexInternal;
template <typename View>
struct OpMaker<OpSetIndexInternal<View>> {
    static ExprRef<View> make(ExprRef<SetView> set, UInt index);
};

template <typename View>
struct OpCatchUndef;
template <typename View>
struct OpMaker<OpCatchUndef<View>> {
    static ExprRef<View> make(ExprRef<View> expr, ExprRef<View> replacement);
};
template <typename SequenceInnerType>
struct OpFlattenOneLevel;

template <typename SequenceInnerType>
struct OpMaker<OpFlattenOneLevel<SequenceInnerType>> {
    static ExprRef<SequenceView> make(ExprRef<SequenceView> o);
};
struct OpAllDiff;
template <>
struct OpMaker<OpAllDiff> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};
struct OpDiv;

template <>
struct OpMaker<OpDiv> {
    static ExprRef<IntView> make(ExprRef<IntView> l, ExprRef<IntView> r);
};

template <typename View>
struct OpTogether;

template <typename View>
struct OpMaker<OpTogether<View>> {
    static ExprRef<BoolView> make(ExprRef<PartitionView> partition,
                                  ExprRef<View> left, ExprRef<View> right);
};

template <typename view>
struct OpInDomain;
template <>
struct OpInDomain<IntView>;

template <typename View>
struct OpMaker<OpInDomain<View>> {
    typedef typename AssociatedValueType<View>::type Value;
    typedef typename AssociatedDomain<Value>::type Domain;
    static ExprRef<BoolView> make(std::shared_ptr<Domain>, ExprRef<View>) {
        todoImpl();
    }
};

template <>
struct OpMaker<OpInDomain<IntView>> {
    static ExprRef<BoolView> make(std::shared_ptr<IntDomain>,
                                  ExprRef<IntView> o);
};

template <>
struct OpMaker<OpInDomain<EnumView>> {
    static ExprRef<BoolView> make(std::shared_ptr<EnumDomain>,
                                  ExprRef<EnumView> o) {
        return OpMaker<OpIsDefined<EnumView>>::make(o);
    }
};
struct OpSetIntersect;
template <>
struct OpMaker<OpSetIntersect> {
    static ExprRef<SetView> make(ExprRef<SetView> l, ExprRef<SetView> r);
};
struct OpFunctionLitBasic;
template <>
struct OpMaker<OpFunctionLitBasic> {
    template <typename RangeViewType>
    static EnableIfViewAndReturn<RangeViewType, ExprRef<FunctionView>> make(
        AnyDomainRef preImageDomain);
};

struct OpAbs;
template <>
struct OpMaker<OpAbs> {
    static ExprRef<IntView> make(ExprRef<IntView> o);
};

struct OpAmplifyConstraint;
template <>
struct OpMaker<OpAmplifyConstraint> {
    static ExprRef<BoolView> make(ExprRef<BoolView> o, UInt64 multiplier);
};

struct OpMsetSubsetEq;
template <>
struct OpMaker<OpMsetSubsetEq> {
    static ExprRef<BoolView> make(ExprRef<MSetView> l, ExprRef<MSetView> r);
};

#endif /* SRC_OPERATORS_OPERATORMAKERS_H_ */
