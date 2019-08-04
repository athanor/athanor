#include <algorithm>
#include "parsing/parserCommon.h"
#include "types/intVal.h"
#include "types/sequenceVal.h"
using namespace std;
using namespace lib;
using namespace nlohmann;

shared_ptr<SequenceDomain> parseDomainSequence(json& sequenceDomainExpr,
                                               ParsedModel& parsedModel) {
    SizeAttr sizeAttr = parseSizeAttr(sequenceDomainExpr[1][0], parsedModel);
    if (sequenceDomainExpr[1][1] == "JectivityAttr_None") {
        return make_shared<SequenceDomain>(
            sizeAttr, parseDomain(sequenceDomainExpr[2], parsedModel));
    } else if (sequenceDomainExpr[1][1] == "JectivityAttr_Injective") {
        return make_shared<SequenceDomain>(
            sizeAttr, parseDomain(sequenceDomainExpr[2], parsedModel), true);
    } else {
        cerr << "Not sure what this attribute for domain "
                "sequence is:\n"
             << sequenceDomainExpr[1][1] << endl;
        abort();
    }
}

shared_ptr<IntDomain> parseDomainInt(json& intDomainExpr,
                                     ParsedModel& parsedModel);

shared_ptr<SequenceDomain> parseDomainMatrix(json& matrixDomainExpr,
                                             ParsedModel& parsedModel) {
    auto indexingDomain = parseDomain(matrixDomainExpr[0], parsedModel);
    auto indexingDomainIntTest =
        mpark::get_if<shared_ptr<IntDomain>>(&indexingDomain);
    if (!indexingDomainIntTest) {
        cerr << "Error: matrices must be indexed by int domains.\n";
        abort();
    }
    return SequenceDomain::makeMatrixDomain(
        *indexingDomainIntTest, parseDomain(matrixDomainExpr[1], parsedModel));
}

ExprRef<IntView> makeIndexShift(ExprRef<IntView> index, Int indexOffset) {
    auto shift = make<IntValue>();
    shift->value = indexOffset;
    shift->setConstant(true);
    auto op = OpMaker<OpMinus>::make(index,shift.asExpr());
    op->setConstant(index->isConstant());
    return op;
}
ParseResult parseOpSequenceIndex(AnyDomainRef& innerDomain,
                                 ExprRef<SequenceView>& sequence,
                                 json& indexExpr, bool hasEmptyType,
                                 Int indexOffset, ParsedModel& parsedModel) {
    auto index =
        expect<IntView>(parseExpr(indexExpr, parsedModel).expr, [](auto&&) {
            cerr << "Sequence must be indexed by an int "
                    "expression.\n";
        });
    if (indexOffset != 0) {
        index = makeIndexShift(index, indexOffset);
    }
    return mpark::visit(
        [&](auto& innerDomain) -> ParseResult {
            typedef typename BaseType<decltype(innerDomain)>::element_type
                InnerDomainType;
            typedef typename AssociatedViewType<InnerDomainType>::type View;
            bool constant = sequence->isConstant() && index->isConstant();
            auto op = OpMaker<OpSequenceIndex<View>>::make(sequence, index);
            op->setConstant(constant);
            return ParseResult(innerDomain, op, hasEmptyType);
        },
        innerDomain);
}

// currently designed for matrices
ParseResult parseOpSequenceLit(json& sequenceExpr, ParsedModel& parsedModel) {
    auto result = parseAllAsSameType(sequenceExpr, parsedModel);
    size_t numberElements = result.numberElements();
    auto sequence = OpMaker<OpSequenceLit>::make(move(result.exprs));
    sequence->setConstant(result.allConstant);
    auto domain =
        make_shared<SequenceDomain>(exactSize(numberElements), result.domain);
    return ParseResult(domain, sequence, result.hasEmptyType);
}
