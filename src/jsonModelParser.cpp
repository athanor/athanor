#include "jsonModelParser.h"
#include <functional>
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include <utility>
#include "common/common.h"
#include "operators/opAnd.h"
#include "operators/opIntEq.h"
#include "operators/opMod.h"
#include "operators/opOr.h"
#include "operators/opProd.h"
#include "operators/opSetNotEq.h"
#include "operators/opSetSize.h"
#include "operators/opSubsetEq.h"
#include "operators/opSum.h"
#include "operators/quantifier.h"
#include "search/model.h"
#include "types/allTypes.h"

using namespace std;
using namespace nlohmann;
auto fakeIntDomain =
    make_shared<IntDomain>(vector<pair<int64_t, int64_t>>({intBound(0, 0)}));
auto fakeBoolDomain = make_shared<BoolDomain>();
shared_ptr<SetDomain> fakeSetDomain(const AnyDomainRef& ref) {
    return make_shared<SetDomain>(exactSize(0), ref);
}

shared_ptr<MSetDomain> fakeMSetDomain(const AnyDomainRef& ref) {
    return make_shared<MSetDomain>(exactSize(0), ref);
}

ParsedModel::ParsedModel() : builder(make_unique<ModelBuilder>()) {}
typedef function<pair<AnyDomainRef, AnyExprRef>(json&, ParsedModel&)>
    ParseFunction;
pair<bool, pair<AnyDomainRef, AnyExprRef>> stringMatch(
    const vector<pair<string, ParseFunction>>& match, json& essenceExpr,
    ParsedModel& parsedModel) {
    for (auto& matchCase : match) {
        if (essenceExpr.count(matchCase.first)) {
            return make_pair(
                true,
                matchCase.second(essenceExpr[matchCase.first], parsedModel));
        }
    }
    return make_pair(false,
                     make_pair(fakeBoolDomain, ViewRef<BoolView>(nullptr)));
}

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseValue(
    json& essenceExpr, ParsedModel& parsedModel);
pair<bool, AnyDomainRef> tryParseDomain(json& essenceExpr,
                                        ParsedModel& parsedModel);
pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseExpr(
    json& essenceExpr, ParsedModel& parsedModel);

pair<AnyDomainRef, AnyExprRef> parseExpr(json& essenceExpr,
                                         ParsedModel& parsedModel) {
    auto boolConstraintPair = tryParseExpr(essenceExpr, parsedModel);
    if (boolConstraintPair.first) {
        return move(boolConstraintPair.second);
    } else {
        cerr << "Failed to parse expression: " << essenceExpr << endl;
        abort();
    }
}

pair<AnyDomainRef, AnyExprRef> parseValue(json& essenceExpr,
                                          ParsedModel& parsedModel) {
    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return move(boolValuePair.second);
    } else {
        cerr << "Failed to parse value: " << essenceExpr << endl;
        abort();
    }
}

AnyValRef toValRef(const AnyExprRef& op) {
    return mpark::visit(
        [&](auto& ref) -> AnyValRef {
            typedef typename AssociatedValueType<exprType(ref)>::type ValType;
            auto val = make<ValType>();
            val->initFrom(*ref);
            return val;
        },
        op);
}

int64_t parseValueAsInt(json& essenceExpr, ParsedModel& parsedModel) {
    return mpark::get<ValRef<IntValue>>(
               toValRef(parseExpr(essenceExpr, parsedModel).second))
        ->value;
}

AnyDomainRef parseDomain(json& essenceExpr, ParsedModel& parsedModel) {
    auto boolDomainPair = tryParseDomain(essenceExpr, parsedModel);
    if (boolDomainPair.first) {
        return move(boolDomainPair.second);
    } else {
        cerr << "Failed to parse domain: " << essenceExpr << endl;
        abort();
    }
}

pair<shared_ptr<SetDomain>, ViewRef<SetView>> parseConstantSet(
    json& essenceSetConstant, ParsedModel& parsedModel) {
    ValRef<SetValue> val = make<SetValue>();
    if (essenceSetConstant.size() == 0) {
        val->setInnerType<IntView>();
        cerr << "Not sure how to work out type of empty set yet, will handle "
                "this later.";
        todoImpl();
    }
    return mpark::visit(
        [&](auto&& innerDomain) {
            typedef typename AssociatedValueType<typename BaseType<decltype(
                innerDomain)>::element_type>::type InnerValueType;
            val->setInnerType<
                typename AssociatedViewType<InnerValueType>::type>();
            for (size_t i = 0; i < essenceSetConstant.size(); ++i) {
                val->addMember(mpark::get<ValRef<InnerValueType>>(toValRef(
                    parseExpr(essenceSetConstant[i], parsedModel).second)));
            }
            return make_pair(make_shared<SetDomain>(
                                 exactSize(val->numberElements()), innerDomain),
                             getViewPtr(val));
        },
        parseValue(essenceSetConstant[0], parsedModel).first);
}

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseValue(
    json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Constant")) {
        return tryParseValue(essenceExpr["Constant"], parsedModel);
    } else if (essenceExpr.count("ConstantAbstract")) {
        return tryParseValue(essenceExpr["ConstantAbstract"], parsedModel);
    } else if (essenceExpr.count("AbstractLiteral")) {
        return tryParseValue(essenceExpr["AbstractLiteral"], parsedModel);
    } else if (essenceExpr.count("ConstantInt")) {
        auto val = make<IntValue>();
        val->value = essenceExpr["ConstantInt"];
        return make_pair(true, make_pair(fakeIntDomain, getViewPtr(val)));
    } else if (essenceExpr.count("ConstantBool")) {
        auto val = make<BoolValue>();
        val->violation = (bool(essenceExpr["ConstantBool"])) ? 0 : 1;
        return make_pair(true, make_pair(fakeBoolDomain, getViewPtr(val)));
    } else if (essenceExpr.count("AbsLitSet")) {
        return make_pair(
            true, parseConstantSet(essenceExpr["AbsLitSet"], parsedModel));
    } else if (essenceExpr.count("Reference")) {
        auto& essenceReference = essenceExpr["Reference"];
        string referenceName = essenceReference[0]["Name"];
        if (parsedModel.namedExprs.count(referenceName)) {
            return make_pair(true, parsedModel.namedExprs.at(referenceName));
        } else {
            cerr << "Found reference to value with name \"" << referenceName
                 << "\" but this does not appear to be in scope.\n"
                 << essenceExpr << endl;
            abort();
        }
    }
    return make_pair(false,
                     make_pair(AnyDomainRef(shared_ptr<IntDomain>(nullptr)),
                               AnyExprRef(ViewRef<IntView>(nullptr))));
}

shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel) {
    vector<pair<int64_t, int64_t>> ranges;
    for (auto& rangeExpr : intDomainExpr) {
        int64_t from, to;

        if (rangeExpr.count("RangeBounded")) {
            from = parseValueAsInt(rangeExpr["RangeBounded"][0], parsedModel);
            to = parseValueAsInt(rangeExpr["RangeBounded"][1], parsedModel);
        } else if (rangeExpr.count("RangeSingle")) {
            from = parseValueAsInt(rangeExpr["RangeSingle"], parsedModel);
            to = from;
        } else {
            cerr << "Unrecognised type of int range: " << rangeExpr << endl;
            abort();
        }
        ranges.emplace_back(from, to);
    }
    return make_shared<IntDomain>(move(ranges));
}

shared_ptr<BoolDomain> parseDomainBool(json&, ParsedModel&) {
    return fakeBoolDomain;
}

SizeAttr parseSizeAttr(json& sizeAttrExpr, ParsedModel& parsedModel) {
    if (sizeAttrExpr.count("SizeAttr_None")) {
        return noSize();
    } else if (sizeAttrExpr.count("SizeAttr_MinSize")) {
        return minSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_MinSize"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_MaxSize")) {
        return maxSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_MaxSize"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_Size")) {
        return exactSize(
            parseValueAsInt(sizeAttrExpr["SizeAttr_Size"], parsedModel));
    } else if (sizeAttrExpr.count("SizeAttr_MinMaxSize")) {
        auto& sizeRangeExpr = sizeAttrExpr["SizeAttr_MinMaxSize"];
        return sizeRange(parseValueAsInt(sizeRangeExpr[0], parsedModel),
                         parseValueAsInt(sizeRangeExpr[1], parsedModel));
    } else {
        cerr << "Could not parse this as a size attribute: " << sizeAttrExpr
             << endl;
        abort();
    }
}

shared_ptr<SetDomain> parseDomainSet(json& setDomainExpr,
                                     ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(setDomainExpr[1], parsedModel);
    return make_shared<SetDomain>(sizeAttr,
                                  parseDomain(setDomainExpr[2], parsedModel));
}

shared_ptr<MSetDomain> parseDomainMSet(json& mSetDomainExpr,
                                       ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(mSetDomainExpr[1][0], parsedModel);
    if (!mSetDomainExpr[1][1].count("OccurAttr_None")) {
        cerr << "Error: for the moment, given attribute must be "
                "OccurAttr_None.  This is not handled yet: "
             << mSetDomainExpr[1][1] << endl;
        abort();
    }
    return make_shared<MSetDomain>(sizeAttr,
                                   parseDomain(mSetDomainExpr[2], parsedModel));
}

pair<bool, AnyDomainRef> tryParseDomain(json& domainExpr,
                                        ParsedModel& parsedModel) {
    if (domainExpr.count("DomainInt")) {
        return make_pair(true,
                         parseDomainInt(domainExpr["DomainInt"], parsedModel));
    } else if (domainExpr.count("DomainBool")) {
        return make_pair(
            true, parseDomainBool(domainExpr["DomainBool"], parsedModel));
    } else if (domainExpr.count("DomainSet")) {
        return make_pair(true,
                         parseDomainSet(domainExpr["DomainSet"], parsedModel));
    } else if (domainExpr.count("DomainMSet")) {
        return make_pair(
            true, parseDomainMSet(domainExpr["DomainMSet"], parsedModel));
    } else if (domainExpr.count("DomainReference")) {
        auto& domainReference = domainExpr["DomainReference"];
        string referenceName = domainReference[0]["Name"];
        if (!parsedModel.domainLettings.count(referenceName)) {
            cerr << "Found reference to domainwith name \"" << referenceName
                 << "\" but this does not appear to be in scope.\n"
                 << domainExpr << endl;
            abort();
        } else {
            return make_pair(true,
                             parsedModel.domainLettings.at(referenceName));
        }
    }
    return make_pair(false, AnyDomainRef(shared_ptr<IntDomain>(nullptr)));
}

template <typename RetType, typename Constraint, typename Func>
ExprRef<RetType> expect(Constraint&& constraint, Func&& func) {
    return mpark::visit(
        overloaded(
            [&](ExprRef<RetType>&& constraint) -> ExprRef<RetType> {
                return move(constraint);
            },
            [&](auto&& constraint) -> ExprRef<RetType> {
                func(forward<decltype(constraint)>(constraint));
                cerr << "\nType found was instead "
                     << TypeAsString<typename AssociatedValueType<exprType(
                            constraint)>::type>::value
                     << endl;
                abort();
            }),
        forward<Constraint>(constraint));
}

pair<AnyDomainRef, AnyExprRef> parseOpMod(json& modExpr,
                                          ParsedModel& parsedModel) {
    string errorMessage = "Expected int returning expression within Op mod: ";
    ExprRef<IntView> left =
        expect<IntView>(parseExpr(modExpr[0], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << modExpr[0]; });
    ExprRef<IntView> right =
        expect<IntView>(parseExpr(modExpr[1], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << modExpr[1]; });
    return make_pair(fakeIntDomain, ViewRef<IntView>(make_shared<OpMod>(
                                        move(left), move(right))));
}

pair<AnyDomainRef, AnyExprRef> parseOpSubsetEq(json& subsetExpr,
                                               ParsedModel& parsedModel) {
    string errorMessage =
        "Expected set returning expression within Op subset: ";
    ExprRef<SetView> left =
        expect<SetView>(parseExpr(subsetExpr[0], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << subsetExpr[0]; });
    ExprRef<SetView> right =
        expect<SetView>(parseExpr(subsetExpr[1], parsedModel).second,
                        [&](auto&&) { cerr << errorMessage << subsetExpr[1]; });
    return make_pair(fakeBoolDomain, ViewRef<BoolView>(make_shared<OpSubsetEq>(
                                         move(left), move(right))));
}

pair<AnyDomainRef, AnyExprRef> parseOpTwoBars(json& operandExpr,
                                              ParsedModel& parsedModel) {
    AnyExprRef operand = parseExpr(operandExpr, parsedModel).second;
    return mpark::visit(
        overloaded(
            [&](ExprRef<SetView>&& set)
                -> pair<shared_ptr<IntDomain>, AnyExprRef> {
                return make_pair(fakeIntDomain,
                                 ViewRef<IntView>(make_shared<OpSetSize>(set)));
            },
            [&](auto&& operand) -> pair<shared_ptr<IntDomain>, AnyExprRef> {
                cerr << "Error, not yet handling OpTwoBars with an operand "
                        "of type "
                     << TypeAsString<
                            typename AssociatedValueType<typename BaseType<
                                decltype(operand)>::element_type>::type>::value
                     << ": " << operandExpr << endl;
                abort();
            }),
        operand);
}

pair<AnyDomainRef, AnyExprRef> parseOpEq(json& operandsExpr,
                                         ParsedModel& parsedModel) {
    AnyExprRef left = parseExpr(operandsExpr[0], parsedModel).second;
    AnyExprRef right = parseExpr(operandsExpr[1], parsedModel).second;
    return mpark::visit(
        [&](auto& left) {
            auto errorHandler = [&](auto&&) {
                cerr << "Expected right operand to be of same type as left, "
                        "i.e. "
                     << TypeAsString<typename AssociatedValueType<exprType(
                            left)>::type>::value
                     << endl;
            };
            return overloaded(
                [&](ExprRef<IntView>&& left)
                    -> pair<shared_ptr<BoolDomain>, ViewRef<BoolView>> {
                    return make_pair(
                        fakeBoolDomain,
                        ViewRef<BoolView>(make_shared<OpIntEq>(
                            left, expect<IntView>(right, errorHandler))));
                },
                [&](auto&& left)
                    -> pair<shared_ptr<BoolDomain>, ViewRef<BoolView>> {
                    cerr
                        << "Error, not yet handling OpEq with operands of type "
                        << TypeAsString<typename AssociatedValueType<exprType(
                               left)>::type>::value
                        << ": " << operandsExpr << endl;
                    abort();
                })(left);
        },
        left);
}

template <typename ExprType>
shared_ptr<FixedArray<ExprType>> parseConstantMatrix(json& matrixExpr,
                                                     ParsedModel& parsedModel) {
    vector<ExprRef<ExprType>> elements;

    for (auto& elementExpr : matrixExpr[1]) {
        ExprRef<ExprType> element = expect<ExprType>(
            parseExpr(elementExpr, parsedModel).second, [&](auto&&) {
                cerr << "Error whilst parsing one of the elements to a "
                        "constant matrix: "
                     << elementExpr << endl;
            });
        elements.emplace_back(move(element));
    }
    return make_shared<FixedArray<ExprType>>(move(elements));
}

template <typename ExprType, typename ContainerDomainType, typename Quantifier>
void addExprToQuantifier(json& comprExpr,
                         shared_ptr<ContainerDomainType>& containerDomain,
                         Quantifier& quantifier, ParsedModel& parsedModel) {
    auto& generatorExpr = comprExpr[1][0]["Generator"]["GenInExpr"];
    string name;
    mpark::visit(
        [&](auto& innerDomain) {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedValueType<InnerDomainType>::type
                InnerValueType;
            name = generatorExpr[0]["Single"]["Name"];
            auto iter = quantifier->template newIterRef<InnerValueType>();
            parsedModel.namedExprs.emplace(name, make_pair(innerDomain, iter));
        },
        (containerDomain->inner));
    quantifier->setExpression(expect<ExprType>(
        parseExpr(comprExpr[0], parsedModel).second, [&](auto&&) {}));
    parsedModel.namedExprs.erase(name);
}

template <typename ExprType, typename ContainerReturnType,
          typename ContainerDomainPtrType>
auto buildQuant(json& comprExpr, ContainerReturnType& container,
                ContainerDomainPtrType&& domain, ParsedModel& parsedModel) {
    auto quantifier =
        make_shared<Quantifier<exprType(container), ExprType>>(container);
    addExprToQuantifier<ExprType>(comprExpr, domain, quantifier, parsedModel);
    return quantifier;
};

template <typename ExprType>
shared_ptr<QuantifierView<ExprType>> parseComprehension(
    json& comprExpr, ParsedModel& parsedModel) {
    auto& generatorExpr = comprExpr[1][0]["Generator"]["GenInExpr"];

    auto errorHandler = [&](auto &&) -> shared_ptr<QuantifierView<ExprType>> {
        cerr << "Error, not yet handling quantifier for this type: "
             << generatorExpr << endl;
        abort();
    };
    auto domainContainerPair = parseExpr(generatorExpr[1], parsedModel);
    return mpark::visit(
        overloaded(
            [&](ExprRef<SetView>&& set)
                -> shared_ptr<QuantifierView<ExprType>> {
                auto& domain = mpark::get<shared_ptr<SetDomain>>(
                    domainContainerPair.first);

                return buildQuant<ExprType>(comprExpr, set, domain,
                                            parsedModel);
            },
            [&](ExprRef<MSetView>&& mSet)
                -> shared_ptr<QuantifierView<ExprType>> {
                auto& domain = mpark::get<shared_ptr<MSetDomain>>(
                    domainContainerPair.first);

                return buildQuant<ExprType>(comprExpr, mSet, domain,
                                            parsedModel);
            },
            move(errorHandler)),
        domainContainerPair.second);
}
template <typename ExprType>
shared_ptr<QuantifierView<ExprType>> parseQuantifierOrMatrix(
    json& expr, ParsedModel& parsedModel) {
    if (expr.count("AbstractLiteral")) {
        if (expr["AbstractLiteral"].count("AbsLitMatrix")) {
            return parseConstantMatrix<ExprType>(
                expr["AbstractLiteral"]["AbsLitMatrix"], parsedModel);
        }
    } else if (expr.count("Comprehension")) {
        return parseComprehension<ExprType>(expr["Comprehension"], parsedModel);
    }
    cerr << "Not sure how to parse this type within the context of an "
            "argument list, expected constant matrix or quantifier.\n"
         << expr << endl;
    abort();
}

template <typename View, typename Op, typename Domain>
auto makeVaradicOpParser(const Domain& domain) {
    return [&](json& essenceExpr,
               ParsedModel& parsedModel) -> pair<AnyDomainRef, AnyExprRef> {
        return make_pair(
            domain, ViewRef<View>(make_shared<Op>(parseQuantifierOrMatrix<View>(
                        essenceExpr, parsedModel))));
    };
}

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseExpr(
    json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Op")) {
        auto boolExprPair = stringMatch(
            {
                {"MkOpEq", parseOpEq},              //
                {"MkOpMod", parseOpMod},            //
                {"MkOpTwoBars", parseOpTwoBars},    //
                {"MkOpSubsetEq", parseOpSubsetEq},  //
                {"MkOpAnd",
                 makeVaradicOpParser<BoolView, OpAnd>(fakeBoolDomain)},  //
                {"MkOpOr",
                 makeVaradicOpParser<BoolView, OpOr>(fakeBoolDomain)},  //
                {"MkOpSum",
                 makeVaradicOpParser<IntView, OpSum>(fakeIntDomain)},  //
                {"MkOpProduct",
                 makeVaradicOpParser<IntView, OpProd>(fakeIntDomain)},  //

            },  //
            essenceExpr["Op"], parsedModel);
        if (boolExprPair.first) {
            return boolExprPair;
        }
    }

    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return boolValuePair;
    }
    return make_pair(false,
                     make_pair(fakeBoolDomain, ViewRef<BoolView>(nullptr)));
}

void handleLettingDeclaration(json& lettingArray, ParsedModel& parsedModel) {
    string lettingName = lettingArray[0]["Name"];
    auto boolValuePair = tryParseValue(lettingArray[1], parsedModel);
    if (boolValuePair.first) {
        parsedModel.namedExprs.emplace(lettingName, boolValuePair.second);
        return;
    }
    if (lettingArray[1].count("Domain")) {
        auto domain = parseDomain(lettingArray[1]["Domain"], parsedModel);
        parsedModel.domainLettings.emplace(lettingName, domain);
        return;
    }
    cerr << "Not sure how to parse this letting: " << lettingArray << endl;
}

void handleFindDeclaration(json& findArray, ParsedModel& parsedModel) {
    string findName = findArray[1]["Name"];
    auto findDomain = parseDomain(findArray[2], parsedModel);
    mpark::visit(
        [&](auto& domainImpl) {
            parsedModel.namedExprs.emplace(
                findName, make_pair(domainImpl,
                                    getViewPtr(parsedModel.builder->addVariable(
                                        findName, domainImpl))));
        },
        findDomain);
}

void parseExprs(json& suchThat, ParsedModel& parsedModel) {
    for (auto& op : suchThat) {
        ExprRef<BoolView> constraint =
            expect<BoolView>(parseExpr(op, parsedModel).second, [&](auto&&) {
                cerr << "Expected Bool returning constraint within such that: "
                     << op << endl;
            });
        parsedModel.builder->addConstraint(constraint);
    }
}

inline void parseObjective(json& objExpr, ParsedModel& parsedModel) {
    string modeStr = objExpr[0];
    OptimiseMode mode = (modeStr == "Minimising") ? OptimiseMode::MINIMISE
                                                  : OptimiseMode::MAXIMISE;

    ExprRef<IntView> objConstraint =
        expect<IntView>(parseExpr(objExpr[1], parsedModel).second, [&](auto&&) {
            cerr << "Expected int returning expression for objective: "
                 << objExpr << endl;
        });
    parsedModel.builder->setObjective(mode, objConstraint);
}
ParsedModel parsedModelFromJson(istream& is) {
    json j;
    is >> j;
    ParsedModel parsedModel;
    for (auto& statement : j["mStatements"]) {
        if (statement.count("Declaration")) {
            auto& declaration = statement["Declaration"];
            if (declaration.count("Letting")) {
                handleLettingDeclaration(declaration["Letting"], parsedModel);
            } else if (declaration.count("FindOrGiven") &&
                       declaration["FindOrGiven"][0] == "Find") {
                handleFindDeclaration(declaration["FindOrGiven"], parsedModel);
            }
        } else if (statement.count("SuchThat")) {
            parseExprs(statement["SuchThat"], parsedModel);
        } else if (statement.count("Objective")) {
            parseObjective(statement["Objective"], parsedModel);
        }
    }
    return parsedModel;
}
