#include <algorithm>
#include "operators/opSetLit.h"
#include "parsing/parserCommon.h"
#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
namespace {

auto fakeBoolDomain = make_shared<BoolDomain>();
}
ParseResult parseConstantBool(json& boolExpr, ParsedModel&) {
    auto val = make<BoolValue>();
    val->violation = (bool(boolExpr)) ? 0 : 1;
    val->setConstant(true);
    return ParseResult(make_shared<BoolDomain>(), val.asExpr(), false);
}

shared_ptr<BoolDomain> parseDomainBool(json&, ParsedModel&) {
    return make_shared<BoolDomain>();
}

ParseResult parseOpNot(json& notExpr, ParsedModel& parsedModel) {
    string errorMessage = "Expected bool returning expression within OpNot: ";
    ExprRef<BoolView> operand =
        expect<BoolView>(parseExpr(notExpr, parsedModel).expr,
                         [&](auto&&) { cerr << errorMessage << notExpr[0]; });
    bool constant = operand->isConstant();
    auto op = OpMaker<OpNot>::make(move(operand));
    op->setConstant(constant);
    return ParseResult(fakeBoolDomain, op, false);
}

ParseResult parseOpIn(json& inExpr, ParsedModel& parsedModel) {
    AnyExprRef expr = parseExpr(inExpr[0], parsedModel).expr;
    auto setOperand =
        expect<SetView>(parseExpr(inExpr[1], parsedModel).expr, [&](auto&&) {
            cerr << "OpIn expects a set on the right.\n" << inExpr;
        });
    bool constant = false;
    mpark::visit(
        [&](auto& expr) {
            constant = expr->isConstant() && setOperand->isConstant();
        },
        expr);
    auto op = OpMaker<OpIn>::make(move(expr), move(setOperand));
    op->setConstant(constant);
    return ParseResult(fakeBoolDomain, op, false);
}

ParseResult parseOpSubsetEq(json& subsetExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected set returning expression within Op "
        "subset: ";
    ExprRef<SetView> left =
        expect<SetView>(parseExpr(subsetExpr[0], parsedModel).expr,
                        [&](auto&&) { cerr << errorMessage << subsetExpr[0]; });
    ExprRef<SetView> right =
        expect<SetView>(parseExpr(subsetExpr[1], parsedModel).expr,
                        [&](auto&&) { cerr << errorMessage << subsetExpr[1]; });
    bool constant = left->isConstant() && right->isConstant();
    auto op = OpMaker<OpSubsetEq>::make(move(left), move(right));
    op->setConstant(constant);
    return ParseResult(fakeBoolDomain, op, false);
}

ParseResult parseOpAllDiff(json& operandExpr, ParsedModel& parsedModel) {
    ParseResult parsedOperandExpr =
        toSequence(parseExpr(operandExpr, parsedModel));
    auto sequence = mpark::get<ExprRef<SequenceView>>(parsedOperandExpr.expr);
    bool constant = sequence->isConstant();
    auto op = OpMaker<OpAllDiff>::make(sequence);
    op->setConstant(constant);
    return ParseResult(fakeBoolDomain, op, false);
}

ParseResult parseOpEq(json& operandsExpr, ParsedModel& parsedModel) {
    AnyExprRef leftAnyExpr = parseExpr(operandsExpr[0], parsedModel).expr;
    AnyExprRef rightAnyExpr = parseExpr(operandsExpr[1], parsedModel).expr;
    return mpark::visit(
        [&](auto& left) -> ParseResult {
            auto errorHandler = [&](auto&&) {
                cerr << "Expected right operand to be of "
                        "same type as left, "
                        "i.e. "
                     << TypeAsString<typename AssociatedValueType<viewType(
                            left)>::type>::value
                     << endl;
            };
            auto right = expect<viewType(left)>(rightAnyExpr, errorHandler);

            // adding fake template args to stop functions from being compiled
            // until invoked
            bool constant = left->isConstant() && right->isConstant();
            return overloaded(
                [&](ExprRef<IntView>& left, auto&&) {
                    auto op = OpMaker<OpIntEq>::make(left, right);
                    op->setConstant(constant);
                    return ParseResult(fakeBoolDomain, op, false);
                },

                [&](ExprRef<BoolView>& left, auto&&) {
                    auto op = OpMaker<OpBoolEq>::make(left, right);
                    op->setConstant(constant);
                    return ParseResult(fakeBoolDomain, op, false);
                },
                [&](ExprRef<EnumView>& left, auto&&) {
                    auto op = OpMaker<OpEnumEq>::make(left, right);
                    op->setConstant(constant);
                    return ParseResult(fakeBoolDomain, op, false);
                },
                [&](auto&& left, auto &&) -> ParseResult {
                    cerr << "Error, not yet handling OpEq "
                            "with operands of "
                            "type "
                         << TypeAsString<typename AssociatedValueType<viewType(
                                left)>::type>::value
                         << ": " << operandsExpr << endl;
                    abort();
                })(left, 0);
            // 0 is a fake arg put there to match the auto parameter to the
            // above lambdas
        },
        leftAnyExpr);
}

ParseResult parseOpNotEq(json& operandsExpr, ParsedModel& parsedModel) {
    AnyExprRef left = parseExpr(operandsExpr[0], parsedModel).expr;
    AnyExprRef right = parseExpr(operandsExpr[1], parsedModel).expr;
    return mpark::visit(
        [&](auto& left) {
            auto errorHandler = [&](auto&&) {
                cerr << "Expected right operand to be of "
                        "same type as left, "
                        "i.e. "
                     << TypeAsString<typename AssociatedValueType<viewType(
                            left)>::type>::value
                     << endl;
            };
            typedef viewType(left) View;
            auto rightImpl = expect<View>(right, errorHandler);
            bool constant = left->isConstant() && rightImpl->isConstant();
            auto op = OpMaker<OpNotEq<View>>::make(left, rightImpl);
            op->setConstant(constant);
            return ParseResult(fakeBoolDomain, op, false);
        },
        left);
}

ParseResult parseOpTogether(json& operandsExpr, ParsedModel& parsedModel) {
    const string UNSUPPORTED_MESSAGE =
        "Only supporting together when given a set literal "
        "of two elements "
        "as "
        "the first parameter.\n";
    auto set = expect<SetView>(parseExpr(operandsExpr[0], parsedModel).expr,
                               [&](auto&&) {
                                   cerr << "Error, together takes a set as its "
                                           "first parameter.\n";
                               });

    auto partition = expect<PartitionView>(
        parseExpr(operandsExpr[1], parsedModel).expr, [&](auto&&) {
            cerr << "Error, together takes a partition as "
                    "its second "
                    "parameter.\n";
        });
    auto setLit = getAs<OpSetLit>(set);
    if (!setLit) {
        cerr << UNSUPPORTED_MESSAGE << endl;
        abort();
    }
    return mpark::visit(
        [&](auto& members) {
            if (members.size() != 2) {
                cerr << UNSUPPORTED_MESSAGE << endl;
                abort();
            }

            typedef viewType(members) View;
            bool constant = partition->isConstant() &&
                            members[0]->isConstant() &&
                            members[1]->isConstant();
            auto op = OpMaker<OpTogether<View>>::make(partition, members[0],
                                                      members[1]);
            op->setConstant(constant);
            return ParseResult(fakeBoolDomain, op, false);
        },
        setLit->operands);
}

template <bool flipped>
ParseResult parseOpLess(json& expr, ParsedModel& parsedModel) {
    auto errorFunc = [&](auto&&) {
        cerr << "Expected int within op less\n" << expr << endl;
    };
    auto left =
        expect<IntView>(parseExpr(expr[0], parsedModel).expr, errorFunc);
    auto right =
        expect<IntView>(parseExpr(expr[1], parsedModel).expr, errorFunc);
    if (flipped) {
        swap(left, right);
    }
    auto op = OpMaker<OpLess>::make(left, right);
    op->setConstant(left->isConstant() && right->isConstant());
    return ParseResult(fakeBoolDomain, op, false);
}

template <bool flipped>
ParseResult parseOpLessEq(json& expr, ParsedModel& parsedModel) {
    auto errorFunc = [&](auto&&) {
        cerr << "Expected int within op lessEQ\n" << expr << endl;
    };
    auto left =
        expect<IntView>(parseExpr(expr[0], parsedModel).expr, errorFunc);
    auto right =
        expect<IntView>(parseExpr(expr[1], parsedModel).expr, errorFunc);
    if (flipped) {
        swap(left, right);
    }
    auto op = OpMaker<OpLessEq>::make(left, right);
    op->setConstant(left->isConstant() && right->isConstant());
    return ParseResult(fakeBoolDomain, op, false);
}

template ParseResult parseOpLess<false>(json&, ParsedModel&);
template ParseResult parseOpLess<true>(json&, ParsedModel&);

template ParseResult parseOpLessEq<false>(json&, ParsedModel&);
template ParseResult parseOpLessEq<true>(json&, ParsedModel&);

ParseResult parseOpImplies(json& expr, ParsedModel& parsedModel) {
    auto errorFunc = [&](auto&&) {
        cerr << "Expected bool within OpImplies less\n" << expr << endl;
    };
    auto left =
        expect<BoolView>(parseExpr(expr[0], parsedModel).expr, errorFunc);
    auto right =
        expect<BoolView>(parseExpr(expr[1], parsedModel).expr, errorFunc);
    auto op = OpMaker<OpImplies>::make(left, right);
    op->setConstant(left->isConstant() && right->isConstant());
    return ParseResult(fakeBoolDomain, op, false);
}
