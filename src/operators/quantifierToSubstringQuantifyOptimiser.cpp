
// This file is horible, my apologies.#include "operators/quantifier.h"
#include <vector>

#include "operators/intRange.h"
#include "operators/opAnd.h"
#include "operators/opImplies.h"
#include "operators/opIntEq.h"
#include "operators/opLess.h"
#include "operators/opLessEq.h"
#include "operators/opMinus.h"
#include "operators/opProd.h"
#include "operators/opSequenceIndex.h"
#include "operators/opSequenceLit.h"
#include "operators/opSum.h"
#include "operators/opToInt.h"
#include "operators/opTupleIndex.h"
#include "operators/opTupleLit.h"
#include "operators/operatorMakers.h"
#include "operators/quantifier.h"
#include "triggers/allTriggers.h"
#include "types/allTypes.h"
#include "types/intVal.h"
template <typename View>
struct OpSubstringQuantify;
template <typename View>
struct OpMaker<OpSubstringQuantify<View>> {
    static ExprRef<SequenceView> make(ExprRef<SequenceView> sequence,
                                      ExprRef<IntView> lowerBound,
                                      ExprRef<IntView> upperBound,
                                      size_t windowSize);
};
template <typename View>
struct OpTupleIndex;

using namespace std;

namespace SubstringQuantifyDetail {

bool appendSequenceIndexOp(const PathExtension& path,
                           lib::optional<AnyExprVec>& sequenceIndexOps) {
    const PathExtension* current = &path;
    while (!current->isTop()) {
        bool success = lib::visit(
            [&](auto& expr) -> bool {
                typedef viewType(expr) View;
                auto sequenceIndex = getAs<OpSequenceIndex<View>>(expr);
                if (!sequenceIndex) {
                    return false;
                }
                if (!sequenceIndexOps) {
                    sequenceIndexOps.emplace();
                    sequenceIndexOps->emplace<ExprRefVec<View>>({expr});
                    return true;
                } else {
                    auto ops = lib::get_if<ExprRefVec<View>>(
                        &sequenceIndexOps.value());
                    if (ops) {
                        ops->emplace_back(expr);
                        return true;
                    } else {
                        return false;
                    }
                }
            },
            current->expr);
        if (success) {
            return true;
        }
        current = current->parent;
    }
    return false;
}
template <typename View>
OptionalRef<const OpTupleIndex<View>> getIfTupleIndexOverIterator(
    UInt64 iterId, const ExprRef<View>& expr) {
    auto tupleIndexTest = getAs<OpTupleIndex<View>>(expr);
    if (!tupleIndexTest) {
        return EmptyOptional();
    }
    auto iter = getAs<Iterator<TupleView>>(tupleIndexTest->tupleOperand);
    if (iter && iter->id == iterId) {
        return *tupleIndexTest;
    } else {
        return EmptyOptional();
    }
}

bool isTupleIndexOverIterator(UInt64 iterId, const AnyExprRef& expr) {
    return lib::visit(
        [&](auto& expr) {
            return getIfTupleIndexOverIterator(iterId, expr).hasValue();
        },
        expr);
}

// Helps with using the findAndReplace function to search for iterator nodes.
// make a function that can be given to findAndReplace that does no replacing.
// Instead, it filters for iterator nodes with the specified id and invokes the
// given function on that iterator.
// function filters for tuple index over the iterator, not the iterator itself.
template <typename Func>
auto filterForTupleIndexOverIterator(UInt64 iterId, Func&& func) {
    return [iterId, f = forward<Func>(func)](
               AnyExprRef expr,
               const PathExtension path) -> pair<bool, AnyExprRef> {
        lib::visit(
            [&](auto& expr) {
                auto tupleIndex = getIfTupleIndexOverIterator(iterId, expr);
                if (tupleIndex) {
                    f(*tupleIndex, path);
                }
            },
            expr);
        return make_pair(false, expr);
    };
}

// check that all uses of the iterator, in both the quantifier expr and
// condition, have a sequence index op as an ancestor.  If any don't, return an
// empty optional, otherwise return a vector of such sequence index ops.
template <typename Quantifier>
lib::optional<AnyExprVec> searchForSequenceIndexOps(Quantifier& quant) {
    lib::optional<AnyExprVec> sequenceIndexOps;
    bool fail = false;
    FindAndReplaceFunction sequenceIndexOpFinder =
        filterForTupleIndexOverIterator(
            quant.quantId, [&](auto&, const PathExtension& path) {
                if (fail) {
                    return;
                }
                fail = !appendSequenceIndexOp(path, sequenceIndexOps);
            });

    lib::visit([&](auto& expr) { findAndReplace(expr, sequenceIndexOpFinder); },
               quant.expr);
    if (!fail && quant.condition) {
        findAndReplace(quant.condition, sequenceIndexOpFinder);
    }
    return (!fail) ? sequenceIndexOps : lib::nullopt;
}

bool checkAllSequenceIndicesUsedTheSameSequence(const AnyExprVec& exprs) {
    return lib::visit(
        [&](auto& exprs) {
            typedef viewType(exprs) View;
            if (exprs.empty()) {
                return true;
            }

            auto& firstIndexOp = *getAs<OpSequenceIndex<View>>(exprs.front());
            auto* firstSequence = &(*(firstIndexOp.sequenceOperand));
            for (size_t i = 1; i < exprs.size(); i++) {
                auto& indexOp = *getAs<OpSequenceIndex<View>>(exprs[i]);
                auto* sequence = &(*(indexOp.sequenceOperand));
                if (sequence != firstSequence) {
                    return false;
                }
            }
            return true;
        },
        exprs);
}

// calculate the index offset from an integer expr.
// The expr may either be the iterator itself, in which case the offset is 0.
// The index operand may instead consist of a sum or minus operator, who's
// operands consist of the iterator and some constants. The constants
// form the offset (positive for OpSum operands, negative for minus operands).
lib::optional<Int> calcIndexOffset(UInt64 iterId,
                                   const ExprRef<IntView>& indexOperand) {
    if (isTupleIndexOverIterator(iterId, indexOperand)) {
        return 0;
    }
    auto minus = getAs<OpMinus>(indexOperand);
    OptionalRef<IntView> minusRightOperand;
    if (minus) {
        if (isTupleIndexOverIterator(iterId, minus->left) &&
            minus->right->isConstant()) {
            minus->right->evaluate();
            minusRightOperand = minus->right->getViewIfDefined();
        }
        return (minusRightOperand)
                   ? lib::make_optional<Int>(-(minusRightOperand->value))
                   : lib::nullopt;
    }
    auto sum = getAs<OpSum>(indexOperand);
    if (sum) {
        bool foundIterator = false;
        auto sequenceLit = getAs<OpSequenceLit>(sum->operand);
        if (!sequenceLit) {
            return lib::nullopt;
        }
        Int offset = 0;
        for (auto& expr : lib::get<ExprRefVec<IntView>>(sequenceLit->members)) {
            if (isTupleIndexOverIterator(iterId, expr)) {
                if (!foundIterator) {
                    foundIterator = true;
                } else {
                    return lib::nullopt;  // Iterator may only exist once.
                }
            } else if (expr->isConstant()) {
                expr->evaluate();
                auto view = expr->getViewIfDefined();
                if (view) {
                    offset += view->value;
                } else {
                    return lib::nullopt;
                }
            } else {
                return lib::nullopt;
            }
        }
        return (foundIterator) ? lib::optional<Int>(offset) : lib::nullopt;
    }
    return lib::nullopt;
}

lib::optional<vector<Int>> calcIndexOffsets(UInt64 iterId,
                                            AnyExprVec& sequenceIndexOps) {
    return lib::visit(
        [&](auto& sequenceIndexOps) -> lib::optional<vector<Int>> {
            typedef viewType(sequenceIndexOps) View;
            vector<Int> offsets;
            for (auto& op : sequenceIndexOps) {
                auto& indexOperand =
                    getAs<OpSequenceIndex<View>>(op)->indexOperand;
                auto offset = calcIndexOffset(iterId, indexOperand);
                if (offset) {
                    offsets.emplace_back(*offset);
                } else {
                    return lib::nullopt;
                }
            }
            return offsets;
        },
        sequenceIndexOps);
}
template <typename View>
unordered_map<OpSequenceIndex<View>*, Int> makePtrsOffsetMap(
    ExprRefVec<View>& sequenceIndexOps, const vector<Int>& offsets) {
    unordered_map<OpSequenceIndex<View>*, Int> ptrs;
    for (size_t i = 0; i < sequenceIndexOps.size(); i++) {
        OpSequenceIndex<View>& op =
            *getAs<OpSequenceIndex<View>>(sequenceIndexOps[i]);
        ptrs.emplace(&op, offsets[i]);
    }
    return ptrs;
}

template <typename View>
auto makeReplaceFunc(
    const unordered_map<OpSequenceIndex<View>*, Int>& ptrsOffsetsMap,
    IterRef<TupleView>& newIterator) {
    return [&](auto expr, const auto&) -> pair<bool, AnyExprRef> {
        auto exprViewTest = lib::get_if<ExprRef<View>>(&expr);
        if (!exprViewTest) {
            return make_pair(false, expr);
        }
        auto sequenceIndexTest = getAs<OpSequenceIndex<View>>(*exprViewTest);
        if (!sequenceIndexTest ||
            !ptrsOffsetsMap.count(&(*sequenceIndexTest))) {
            return make_pair(false, expr);
        }
        auto window = OpMaker<OpTupleIndex<TupleView>>::make(newIterator, 1);
        return make_pair(true,
                         OpMaker<OpTupleIndex<View>>::make(
                             window, ptrsOffsetsMap.at(&(*sequenceIndexTest))));
    };
}

static ExprRef<IntView> addConst(ExprRef<IntView> val, Int add) {
    auto constVal = make<IntValue>();
    constVal->value = add;
    auto op = OpMaker<OpSum>::make(OpMaker<OpSequenceLit>::make(
        ExprRefVec<IntView>({val, constVal.asExpr()})));
    op->setConstant(val->isConstant() && constVal->isConstant());
    return op;
}

template <typename Quant>
void completeOptimiseToSubstringQuantifier(Quant& quant, IntRange& intRange,
                                           AnyExprVec& sequenceIndexOps,
                                           vector<Int> offsets) {
    Int minOffset = *min_element(offsets.begin(), offsets.end());
    Int maxOffset = *max_element(offsets.begin(), offsets.end());
    Int windowSize = (maxOffset - minOffset) + 1;
    // rewrite all the offsets into tuple indices, these are indices
    // into the windows (tuples) produced by the substring quantifier. minOffset
    // is index 0, minOffset + 1 is index 1 and so on
    for_each(offsets.begin(), offsets.end(),
             [&](Int& offset) { offset = offset - minOffset; });

    lib::visit(
        [&](auto& sequenceIndexOps) {
            debug_code(assert(offsets.size() == sequenceIndexOps.size()));

            typedef viewType(sequenceIndexOps) View;
            auto& sequence =
                getAs<OpSequenceIndex<View>>(sequenceIndexOps.front())
                    ->sequenceOperand;
            auto lowerBound = (maxOffset > 0)
                                  ? addConst(intRange.left, maxOffset)
                                  : intRange.left;
            auto upperBound = (maxOffset > 0)
                                  ? addConst(intRange.right, maxOffset)
                                  : intRange.right;
            quant.container = OpMaker<OpSubstringQuantify<View>>::make(
                sequence, lowerBound, upperBound, windowSize);
            auto ptrsOffsetsMap = makePtrsOffsetMap(sequenceIndexOps, offsets);
            auto newIterator = quant.template newIterRef<TupleView>();
            FindAndReplaceFunction replaceFunc =
                makeReplaceFunc(ptrsOffsetsMap, newIterator);
            lib::visit(
                [&](auto& expr) { expr = findAndReplace(expr, replaceFunc); },
                quant.expr);
            if (quant.condition) {
                quant.condition = findAndReplace(quant.condition, replaceFunc);
            }
        },
        sequenceIndexOps);
}
}  // namespace SubstringQuantifyDetail
template <typename View>
bool optimiseIfCanBeConvertedToSubstringQuantifier(Quantifier<View>&);
template <>
bool optimiseIfCanBeConvertedToSubstringQuantifier<SequenceView>(
    Quantifier<SequenceView>& quant) {
    using namespace SubstringQuantifyDetail;
    auto intRangeTest = getAs<IntRange>(quant.container);
    if (!intRangeTest) {
        return false;
    }
    lib::optional<AnyExprVec> sequenceIndexOps =
        searchForSequenceIndexOps(quant);
    if (!sequenceIndexOps ||
        !checkAllSequenceIndicesUsedTheSameSequence(*sequenceIndexOps) ||
        lib::visit([&](auto& ops) { return ops.empty(); }, *sequenceIndexOps)) {
        return false;
    }
    lib::optional<vector<Int>> offsets =
        calcIndexOffsets(quant.quantId, *sequenceIndexOps);
    if (!offsets) {
        return false;
    }
    completeOptimiseToSubstringQuantifier(quant, *intRangeTest,
                                          *sequenceIndexOps, *offsets);
    debug_log(
        "Optimise: rewriting sequence indexing quantifier to substring "
        "quantifier.");
    return true;
}

template <typename View>
bool optimiseIfIndicesAreNotUsedInSequenceQuantifier(Quantifier<View>&) {
    return false;
}

template <>
bool optimiseIfIndicesAreNotUsedInSequenceQuantifier<SequenceView>(
    Quantifier<SequenceView>& quant) {
    using namespace SubstringQuantifyDetail;
    bool canBeOptimised = true;
    bool found = false;
    auto checker = filterForTupleIndexOverIterator(
        quant.quantId, [&](auto& tupleIndex, const auto&) {
            canBeOptimised &= tupleIndex.indexOperand == 1;

            found |= tupleIndex.indexOperand == 1;
            ;
        });
    lib::visit([&](auto& expr) { findAndReplace(expr, checker); }, quant.expr);
    canBeOptimised &= found;

    if (canBeOptimised && quant.condition) {
        findAndReplace(quant.condition, checker);
    }
    if (canBeOptimised != quant.optimisedToNotUpdateIndices) {
        quant.optimisedToNotUpdateIndices = canBeOptimised;
        if (canBeOptimised) {
            debug_log(
                "Optimise: quantifier over sequences will not update yielded "
                "indices.");
        } else {
            debug_log(
                "Optimise: turned off optimisation for not updating indices in "
                "sequence quantifier.");
        }
        return true;
    } else {
        return false;
    }
}
