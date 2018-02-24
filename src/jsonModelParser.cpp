#include "jsonModelParser.h"
#include <iostream>
#include <json.hpp>
#include <unordered_map>
#include "common/common.h"
#include "constructorShortcuts.h"
#include "search/searchStrategies.h"
#include "types/typeOperations.h"

using namespace std;
using namespace nlohmann;
auto fakeIntDomain =
    make_shared<IntDomain>(vector<pair<int64_t, int64_t>>({intBound(0, 0)}));
auto fakeBoolDomain = make_shared<BoolDomain>();
shared_ptr<SetDomain> fakeSetDomain(const AnyDomainRef& ref) {
    return make_shared<SetDomain>(exactSize(0), ref);
}

template <typename DestVariant, typename Variant>
DestVariant variantConvert(Variant&& v) {
    return mpark::visit(
        [](auto&& v) -> DestVariant { return forward<decltype(v)>(v); },
        forward<Variant>(v));
}

shared_ptr<MSetDomain> fakeMSetDomain(const AnyDomainRef& ref) {
    return make_shared<MSetDomain>(exactSize(0), ref);
}

ParsedModel::ParsedModel() : builder(make_unique<ModelBuilder>()) {}
pair<bool, pair<AnyDomainRef, AnyValRef>> tryParseValue(
    json& essenceExpr, ParsedModel& parsedModel);
pair<bool, AnyDomainRef> tryParseDomain(json& essenceExpr,
                                        ParsedModel& parsedModel);
pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseExpr(
    json& essenceExpr, ParsedModel& parsedModel);

pair<AnyDomainRef, AnyValRef> parseValue(json& essenceExpr,
                                         ParsedModel& parsedModel) {
    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return move(boolValuePair.second);
    } else {
        cerr << "Failed to parse value: " << essenceExpr << endl;
        abort();
    }
}

int64_t parseValueAsInt(json& essenceExpr, ParsedModel& parsedModel) {
    return mpark::get<ValRef<IntValue>>(
               parseValue(essenceExpr, parsedModel).second)
        ->value;
}

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

AnyDomainRef parseDomain(json& essenceExpr, ParsedModel& parsedModel) {
    auto boolDomainPair = tryParseDomain(essenceExpr, parsedModel);
    if (boolDomainPair.first) {
        return move(boolDomainPair.second);
    } else {
        cerr << "Failed to parse domain: " << essenceExpr << endl;
        abort();
    }
}

pair<shared_ptr<SetDomain>, ValRef<SetValue>> parseConstantSet(
    json& essenceSetConstant, ParsedModel& parsedModel) {
    ValRef<SetValue> val = make<SetValue>();
    if (essenceSetConstant.size() == 0) {
        val->setInnerType<IntValue>();
        cerr << "Not sure how to work out type of empty set yet, will handle "
                "this later.";
        todoImpl();
    }
    return mpark::visit(
        [&](auto&& innerDomain) {
            typedef typename AssociatedValueType<typename BaseType<decltype(
                innerDomain)>::element_type>::type InnerValueType;
            val->setInnerType<InnerValueType>();
            for (size_t i = 0; i < essenceSetConstant.size(); ++i) {
                val->addMember(mpark::get<ValRef<InnerValueType>>(
                    parseValue(essenceSetConstant[i], parsedModel).second));
            }
            return make_pair(make_shared<SetDomain>(
                                 exactSize(val->numberElements()), innerDomain),
                             val);
        },
        parseValue(essenceSetConstant[0], parsedModel).first);
}

pair<AnyDomainRef, AnyValRef> parseAbstractLiteral(json& abstractLit,
                                                   ParsedModel& parsedModel) {
    if (abstractLit.count("AbsLitSet")) {
        return parseConstantSet(abstractLit["AbsLitSet"], parsedModel);
    } else {
        cerr << "Not sure what type of abstract literal this is: "
             << abstractLit << endl;
        abort();
    }
}

pair<AnyDomainRef, AnyValRef> parseConstant(json& essenceConstant,
                                            ParsedModel&) {
    if (essenceConstant.count("ConstantInt")) {
        auto val = make<IntValue>();
        val->value = essenceConstant["ConstantInt"];
        return make_pair(fakeIntDomain, val);
    } else if (essenceConstant.count("ConstantBool")) {
        auto val = make<BoolValue>();
        val->violation = bool(essenceConstant["ConstantBool"]);
        return make_pair(fakeBoolDomain, val);
    } else {
        cerr << "Not sure what type of constant this is: " << essenceConstant
             << endl;
        abort();
    }
}

pair<bool, pair<AnyDomainRef, AnyValRef>> tryParseValue(
    json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Constant")) {
        return make_pair(true,
                         parseConstant(essenceExpr["Constant"], parsedModel));
    } else if (essenceExpr.count("AbstractLiteral")) {
        return make_pair(
            true,
            parseAbstractLiteral(essenceExpr["AbstractLiteral"], parsedModel));
    } else if (essenceExpr.count("Reference")) {
        auto& essenceReference = essenceExpr["Reference"];
        string referenceName = essenceReference[0]["Name"];
        if (parsedModel.vars.count(referenceName)) {
            return make_pair(true, parsedModel.vars.at(referenceName));
        } else {
            cerr << "Found reference to value with name \"" << referenceName
                 << "\" but this does not appear to be in scope.\n"
                 << essenceExpr << endl;
            abort();
        }
    }
    return make_pair(false,
                     make_pair(AnyDomainRef(shared_ptr<IntDomain>(nullptr)),
                               AnyValRef(ValRef<IntValue>(nullptr))));
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
RetType expect(Constraint&& constraint, Func&& func) {
    return mpark::visit(
        [&](auto&& constraint) -> RetType {
            return staticIf<HasReturnType<
                RetType, BaseType<decltype(constraint)>>::value>(
                       [&](auto&& constraint) -> RetType {
                           return forward<decltype(constraint)>(constraint);
                       })
                .otherwise([&](auto&& constraint) -> RetType {
                    func(forward<decltype(constraint)>(constraint));
                    cerr << "\nType found was instead "
                         << TypeAsString<
                                typename ReturnType<typename BaseType<decltype(
                                    constraint)>::element_type>::type>::value
                         << endl;
                    abort();
                })(forward<decltype(constraint)>(constraint));
        },
        forward<Constraint>(constraint));
}

pair<shared_ptr<IntDomain>, shared_ptr<OpMod>> parseOpMod(
    json& modExpr, ParsedModel& parsedModel) {
    string errorMessage = "Expected int returning expression within Op mod: ";
    IntReturning left = expect<IntReturning>(
        parseExpr(modExpr[0], parsedModel).second,
        [&](auto&&) { cerr << errorMessage << modExpr[0]; });
    IntReturning right = expect<IntReturning>(
        parseExpr(modExpr[1], parsedModel).second,
        [&](auto&&) { cerr << errorMessage << modExpr[1]; });
    return make_pair(fakeIntDomain,
                     make_shared<OpMod>(move(left), move(right)));
}

pair<shared_ptr<BoolDomain>, shared_ptr<OpSubsetEq>> parseOpSubsetEq(
    json& subsetExpr, ParsedModel& parsedModel) {
    string errorMessage =
        "Expected set returning expression within Op subset: ";
    SetReturning left = expect<SetReturning>(
        parseExpr(subsetExpr[0], parsedModel).second,
        [&](auto&&) { cerr << errorMessage << subsetExpr[0]; });
    SetReturning right = expect<SetReturning>(
        parseExpr(subsetExpr[1], parsedModel).second,
        [&](auto&&) { cerr << errorMessage << subsetExpr[1]; });
    return make_pair(fakeBoolDomain,
                     make_shared<OpSubsetEq>(move(left), move(right)));
}

pair<shared_ptr<IntDomain>, AnyExprRef> parseOpTwoBars(
    json& operandExpr, ParsedModel& parsedModel) {
    AnyExprRef operand = parseExpr(operandExpr, parsedModel).second;
    return mpark::visit(
        [&](auto& operand) {
            typedef typename ReturnType<BaseType<decltype(operand)>>::type
                OperandReturnType;
            return overloaded(
                [&](SetReturning&& set)
                    -> pair<shared_ptr<IntDomain>, AnyExprRef> {
                    return make_pair(fakeIntDomain,
                                     make_shared<OpSetSize>(set));
                },
                [&](auto &&) -> pair<shared_ptr<IntDomain>, AnyExprRef> {
                    cerr << "Error, not yet handling OpTwoBars with an operand "
                            "of type "
                         << TypeAsString<OperandReturnType>::value << ": "
                         << operandExpr << endl;
                    abort();
                })(OperandReturnType(operand));
        },
        operand);
}

pair<shared_ptr<BoolDomain>, AnyExprRef> parseOpEq(json& operandsExpr,
                                                   ParsedModel& parsedModel) {
    AnyExprRef left = parseExpr(operandsExpr[0], parsedModel).second;
    AnyExprRef right = parseExpr(operandsExpr[1], parsedModel).second;
    return mpark::visit(
        [&](auto& left) {
            typedef typename ReturnType<BaseType<decltype(left)>>::type
                LeftReturnType;
            auto errorHandler = [&](auto&&) {
                cerr << "Expected right operand to be of same type as left, "
                        "i.e. "
                     << TypeAsString<LeftReturnType>::value << endl;
            };
            return overloaded(
                [&](IntReturning&& left)
                    -> pair<shared_ptr<BoolDomain>, AnyExprRef> {
                    return make_pair(
                        fakeBoolDomain,
                        make_shared<OpIntEq>(
                            left, expect<IntReturning>(right, errorHandler)));
                },
                [&](auto &&) -> pair<shared_ptr<BoolDomain>, AnyExprRef> {
                    cerr
                        << "Error, not yet handling OpEq with operands of type "
                        << TypeAsString<LeftReturnType>::value << ": "
                        << operandsExpr << endl;
                    abort();
                })(LeftReturnType(left));
        },
        left);
}

template <typename ExprType>
shared_ptr<FixedArray<ExprType>> parseConstantMatrix(json& matrixExpr,
                                                     ParsedModel& parsedModel) {
    vector<ExprType> elements;

    for (auto& elementExpr : matrixExpr[1]) {
        ExprType element = expect<ExprType>(
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
            parsedModel.scopedVars.emplace(name, make_pair(innerDomain, iter));
        },
        (containerDomain->inner));
    quantifier->setExpression(expect<ExprType>(
        parseExpr(comprExpr[0], parsedModel).second, [&](auto&&) {}));
    parsedModel.scopedVars.erase(name);
}

template <typename ExprType, typename ContainerReturnType,
          typename ContainerDomainPtrType>
auto buildQuant(json& comprExpr, ContainerReturnType& container,
                ContainerDomainPtrType&& domain, ParsedModel& parsedModel) {
    auto quantifier =
        make_shared<Quantifier<ContainerReturnType, ExprType>>(container);
    addExprToQuantifier<ExprType>(comprExpr, domain, quantifier, parsedModel);
    return quantifier;
};

template <typename ExprType>
shared_ptr<QuantifierView<ExprType>> parseComprehension(
    json& comprExpr, ParsedModel& parsedModel) {
    auto& generatorExpr = comprExpr[1][0]["Generator"]["GenInExpr"];

    auto errorHandler = [&](auto&&,
                            auto &&) -> shared_ptr<QuantifierView<ExprType>> {
        cerr << "Error, not yet handling quantifier for this type: "
             << generatorExpr << endl;
        abort();
    };
    auto domainContainerPair = parseExpr(generatorExpr[1], parsedModel);
    return mpark::visit(
        [&](auto& container) {
            typedef typename ReturnType<BaseType<decltype(container)>>::type
                ContainerReturnType;
            return overloaded(
                [&](SetReturning&& set,
                    auto &&) -> shared_ptr<QuantifierView<ExprType>> {
                    auto& domain = mpark::get<shared_ptr<SetDomain>>(
                        domainContainerPair.first);

                    return buildQuant<ExprType>(comprExpr, set, domain,
                                                parsedModel);
                },
                [&](MSetReturning&& mSet,
                    auto &&) -> shared_ptr<QuantifierView<ExprType>> {
                    auto& domain = mpark::get<shared_ptr<MSetDomain>>(
                        domainContainerPair.first);

                    return buildQuant<ExprType>(comprExpr, mSet, domain,
                                                parsedModel);
                },
                move(errorHandler))(ContainerReturnType(container),
                                    ContainerReturnType(container));
        },
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

pair<bool, pair<AnyDomainRef, AnyExprRef>> tryParseExpr(
    json& essenceExpr, ParsedModel& parsedModel) {
    if (essenceExpr.count("Op")) {
        auto& op = essenceExpr["Op"];
        if (op.count("MkOpEq")) {
            return make_pair(true, parseOpEq(op["MkOpEq"], parsedModel));
        }
        if (op.count("MkOpMod")) {
            return make_pair(true, parseOpMod(op["MkOpMod"], parsedModel));
        }
        if (op.count("MkOpTwoBars")) {
            return make_pair(true,
                             parseOpTwoBars(op["MkOpTwoBars"], parsedModel));
        }
        if (op.count("MkOpSubsetEq")) {
            return make_pair(true,
                             parseOpSubsetEq(op["MkOpSubsetEq"], parsedModel));
        }
        if (op.count("MkOpAnd")) {
            return make_pair(
                true, make_pair(fakeBoolDomain,
                                make_shared<OpAnd>(
                                    parseQuantifierOrMatrix<BoolReturning>(
                                        op["MkOpAnd"], parsedModel))));
        }
        if (op.count("MkOpOr")) {
            return make_pair(
                true, make_pair(fakeBoolDomain,
                                make_shared<OpOr>(
                                    parseQuantifierOrMatrix<BoolReturning>(
                                        op["MkOpOr"], parsedModel))));
        }
    }

    if (essenceExpr.count("Reference")) {
        auto& essenceReference = essenceExpr["Reference"];
        string referenceName = essenceReference[0]["Name"];
        if (parsedModel.scopedVars.count(referenceName)) {
            auto& domainVarPair = parsedModel.scopedVars.at(referenceName);
            return make_pair(
                true,
                make_pair(domainVarPair.first,
                          variantConvert<AnyExprRef>(domainVarPair.second)));
        }
    }

    auto boolValuePair = tryParseValue(essenceExpr, parsedModel);
    if (boolValuePair.first) {
        return make_pair(
            true,
            make_pair(boolValuePair.second.first,
                      variantConvert<AnyExprRef>(boolValuePair.second.second)));
    }

    return make_pair(false,
                     make_pair(fakeBoolDomain, ValRef<IntValue>(nullptr)));
}

void handleLettingDeclaration(json& lettingArray, ParsedModel& parsedModel) {
    string lettingName = lettingArray[0]["Name"];
    auto boolValuePair = tryParseValue(lettingArray[1], parsedModel);
    if (boolValuePair.first) {
        parsedModel.vars.emplace(lettingName, boolValuePair.second);
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
            parsedModel.vars.emplace(
                findName,
                make_pair(
                    domainImpl,
                    AnyValRef(parsedModel.builder->addVariable(domainImpl))));
        },
        findDomain);
}

void parseExprs(json& suchThat, ParsedModel& parsedModel) {
    for (auto& op : suchThat) {
        BoolReturning constraint = expect<BoolReturning>(
            parseExpr(op, parsedModel).second, [&](auto&&) {
                cerr << "Expected Bool returning constraint within such that: "
                     << op << endl;
            });
        parsedModel.builder->addConstraint(constraint);
    }
}

ParsedModel parseModelFromJson(istream& is) {
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
        }
    }
    return parsedModel;
}
