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
        myCerr << "Not sure what this attribute for domain "
                  "sequence is:\n"
               << sequenceDomainExpr[1][1] << endl;
        myAbort();
    }
}

ParseResult parseOpSequenceIndex(AnyDomainRef& innerDomain,
                                 ExprRef<SequenceView>& sequence,
                                 json& indexExpr, bool hasEmptyType,
                                 ParsedModel& parsedModel) {
    auto index =
        expect<IntView>(parseExpr(indexExpr, parsedModel).expr, [](auto&&) {
            myCerr << "Sequence must be indexed by an int "
                      "expression.\n";
        });
    return lib::visit(
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
