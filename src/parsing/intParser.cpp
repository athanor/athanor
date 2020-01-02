#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/allVals.h"
using namespace std;
using namespace lib;
using namespace nlohmann;
namespace {

auto fakeIntDomain = make_shared<IntDomain>(vector<pair<Int, Int>>({{0, 0}}));
}
Int parseExprAsInt(AnyExprRef expr, const string& errorMessage) {
    auto intExpr = expect<IntView>(expr, [&](auto&&) {
        myCerr << "Error: Expected int expression " << errorMessage << endl;
    });
    intExpr->evaluate();
    return intExpr->getViewIfDefined()
        .checkedGet(
            "Could not parse this expression into a constant int.  Value is "
            "undefined.\n")
        .value;
}

Int parseExprAsInt(json& essenceExpr, ParsedModel& parsedModel,
                   const string& errorMessage) {
    return parseExprAsInt(parseExpr(essenceExpr, parsedModel).expr,
                          errorMessage);
}

ParseResult parseConstantInt(json& intExpr, ParsedModel&) {
    auto val = make<IntValue>();
    // handle tagged and untagged ints
    if (intExpr.is_primitive()) {
        val->value = intExpr;
    } else {
        val->value = intExpr[1];
    }
    val->setConstant(true);
    auto domain = make_shared<IntDomain>(
        vector<pair<Int, Int>>({{val->value, val->value}}));
    return ParseResult(domain, val.asExpr(), false);
}

shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel) {
    vector<pair<Int, Int>> ranges;
    string errorMessage = "Within context of int domain";
    auto& rangesToParse =
        (intDomainExpr[0].count("TagInt")) ? intDomainExpr[1] : intDomainExpr;
    for (auto& rangeExpr : rangesToParse) {
        Int from, to;

        if (rangeExpr.count("RangeBounded")) {
            from = parseExprAsInt(rangeExpr["RangeBounded"][0], parsedModel,
                                  errorMessage);
            to = parseExprAsInt(rangeExpr["RangeBounded"][1], parsedModel,
                                errorMessage);
        } else if (rangeExpr.count("RangeSingle")) {
            from = parseExprAsInt(rangeExpr["RangeSingle"], parsedModel,
                                  errorMessage);
            to = from;
        } else {
            myCerr << "Unrecognised type of int range: " << rangeExpr << endl;
            myAbort();
        }
        ranges.emplace_back(from, to);
    }
    return make_shared<IntDomain>(move(ranges));
}

ParseResult parseOpMod(json& modExpr, ParsedModel& parsedModel) {
    string errorMessage = "Expected int returning expression within Op mod: ";
    ExprRef<IntView> left =
        expect<IntView>(parseExpr(modExpr[0], parsedModel).expr,
                        [&](auto&&) { myCerr << errorMessage << modExpr[0]; });
    ExprRef<IntView> right =
        expect<IntView>(parseExpr(modExpr[1], parsedModel).expr,
                        [&](auto&&) { myCerr << errorMessage << modExpr[1]; });
    bool constant = left->isConstant() && right->isConstant();
    auto op = OpMaker<OpMod>::make(move(left), move(right));
    op->setConstant(constant);
    return ParseResult(fakeIntDomain, op, false);
}

ParseResult parseOpNegate(json& negateExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected int returning expression within "
        "OpNegate: ";
    ExprRef<IntView> operand = expect<IntView>(
        parseExpr(negateExpr, parsedModel).expr,
        [&](auto&&) { myCerr << errorMessage << negateExpr[0]; });
    bool constant = operand->isConstant();
    auto op = OpMaker<OpNegate>::make(move(operand));
    op->setConstant(constant);

    return ParseResult(fakeIntDomain, op, false);
}

ParseResult parseOpDiv(json& modExpr, ParsedModel& parsedModel) {
    string errorMessage = "Expected int returning expression within Op div: ";
    ExprRef<IntView> left =
        expect<IntView>(parseExpr(modExpr[0], parsedModel).expr,
                        [&](auto&&) { myCerr << errorMessage << modExpr[0]; });
    ExprRef<IntView> right =
        expect<IntView>(parseExpr(modExpr[1], parsedModel).expr,
                        [&](auto&&) { myCerr << errorMessage << modExpr[1]; });
    bool constant = left->isConstant() && right->isConstant();
    auto op = OpMaker<OpDiv>::make(move(left), move(right));
    op->setConstant(constant);
    return ParseResult(fakeIntDomain, op, false);
}

ParseResult parseOpMinus(json& minusExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected int returning expression within Op "
        "minus: ";
    ExprRef<IntView> left = expect<IntView>(
        parseExpr(minusExpr[0], parsedModel).expr,
        [&](auto&&) { myCerr << errorMessage << minusExpr[0]; });
    ExprRef<IntView> right = expect<IntView>(
        parseExpr(minusExpr[1], parsedModel).expr,
        [&](auto&&) { myCerr << errorMessage << minusExpr[1]; });
    bool constant = left->isConstant() && right->isConstant();
    auto op = OpMaker<OpMinus>::make(move(left), move(right));
    op->setConstant(constant);
    return ParseResult(fakeIntDomain, op, false);
}

ParseResult parseOpPower(json& powerExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected int returning expression within Op "
        "power: ";
    ExprRef<IntView> left = expect<IntView>(
        parseExpr(powerExpr[0], parsedModel).expr,
        [&](auto&&) { myCerr << errorMessage << powerExpr[0]; });
    ExprRef<IntView> right = expect<IntView>(
        parseExpr(powerExpr[1], parsedModel).expr,
        [&](auto&&) { myCerr << errorMessage << powerExpr[1]; });
    bool constant = left->isConstant() && right->isConstant();
    ;
    auto op = OpMaker<OpPower>::make(move(left), move(right));
    op->setConstant(constant);
    return ParseResult(fakeIntDomain, op, false);
}

static ParseResult parseDomainSize(const AnyDomainRef& domain) {
    return lib::visit(
        [&](auto& domain) {
            UInt domainSize = getDomainSize(*domain);
            if (domainSize > numeric_limits<Int>().max()) {
                domainSize = numeric_limits<Int>().max();
            }
            auto val = make<IntValue>();
            val->value = domainSize;
            val->setConstant(true);
            return ParseResult(fakeIntDomain, val.asExpr(), false);
        },
        domain);
}

ParseResult parseOpTwoBars(json& operandExpr, ParsedModel& parsedModel) {
    // if we know for sure it is a domain parse it as such
    if (operandExpr.count("Domain")) {
        auto domain = parseDomain(operandExpr["Domain"], parsedModel);
        return parseDomainSize(domain);
    }
    auto domain = tryParseDomain(operandExpr, parsedModel);
    if (domain) {
        return parseDomainSize(*domain);
    }
    auto expr = tryParseExpr(operandExpr, parsedModel);
    if (!expr) {
        myCerr << "Error parsing OpTwoBars, expected domain or expression.\n";
        myAbort();
    }
    auto& operand = expr->expr;
    return lib::visit(
        overloaded(
            [&](ExprRef<IntView>& expr) -> ParseResult {
                auto op = OpMaker<OpAbs>::make(expr);
                op->setConstant(expr->isConstant());
                return ParseResult(fakeIntDomain, op, false);
            },

            [&](ExprRef<SetView>& set) -> ParseResult {
                auto op = OpMaker<OpSetSize>::make(set);
                op->setConstant(set->isConstant());
                return ParseResult(fakeIntDomain, op, false);
            },
            [&](ExprRef<MSetView>& mSet) -> ParseResult {
                auto op = OpMaker<OpMSetSize>::make(mSet);
                op->setConstant(mSet->isConstant());
                return ParseResult(fakeIntDomain, op, false);
            },
            [&](ExprRef<SequenceView>& sequence) -> ParseResult {
                auto op = OpMaker<OpSequenceSize>::make(sequence);
                op->setConstant(sequence->isConstant());
                return ParseResult(fakeIntDomain, op, false);
            },
            [&](auto&& operand) -> ParseResult {
                myCerr << "Error, not yet handling OpTwoBars "
                          "with an operand "
                          "of type "
                       << TypeAsString<typename AssociatedValueType<viewType(
                              operand)>::type>::value
                       << ": " << operandExpr << endl;
                myAbort();
            }),
        operand);
}

ParseResult parseOpToInt(json& expr, ParsedModel& parsedModel) {
    auto operand = expect<BoolView>(
        parseExpr(expr, parsedModel).expr,
        [&](auto&) { myCerr << "OpToInt requires a bool operand.\n"; });
    auto op = OpMaker<OpToInt>::make(operand);
    op->setConstant(operand->isConstant());
    return ParseResult(make_shared<IntDomain>(vector<pair<Int, Int>>({{0, 1}})),
                       op, false);
    ;
}
