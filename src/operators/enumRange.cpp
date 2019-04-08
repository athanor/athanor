#include "operators/enumRange.h"
#include <iostream>
#include <memory>
#include "types/enumVal.h"

using namespace std;

void EnumRange::evaluateImpl() {
    this->silentClear();
    for (UInt i = 0; i < domain->valueNames.size(); i++) {
        auto val = make<EnumValue>();
        val->setConstant(true);
        val->value = i;
        this->addMember(this->numberElements(), val.asExpr());
    }
    this->setConstant(true);
    this->setAppearsDefined(true);
}

void EnumRange::updateVarViolationsImpl(const ViolationContext&,
                                        ViolationContainer&) {
    shouldNotBeCalledPanic;
}

ostream& EnumRange::dumpState(ostream& os) const {
    return os << "EnumRange(" << domain->domainName << ")";
    return os;
}

template <typename Op>
struct OpMaker;
template <>
struct OpMaker<EnumRange> {
    static ExprRef<SequenceView> make(shared_ptr<EnumDomain> d);
};

ExprRef<SequenceView> OpMaker<EnumRange>::make(shared_ptr<EnumDomain> d) {
    return make_shared<EnumRange>(move(d));
}

string EnumRange::getOpName() const {
    return toString("EnumRange(", domain->domainName, ")");
}

void EnumRange::startTriggeringImpl() {}
void EnumRange::stopTriggering() {}

ExprRef<SequenceView> EnumRange::deepCopyForUnrollImpl(
    const ExprRef<SequenceView>& self, const AnyIterRef&) const {
    return self;
}

pair<bool, ExprRef<SequenceView>> EnumRange::optimise(PathExtension path) {
    return make_pair(false, mpark::get<ExprRef<SequenceView>>(path.expr));
}

void EnumRange::findAndReplaceSelf(const FindAndReplaceFunction&) {}
void EnumRange::debugSanityCheckImpl() const {
    sanityEqualsCheck(0, numberUndefined);
    sanityEqualsCheck(domain->valueNames.size(), numberElements());

    for (UInt i = 0; i < (UInt)domain->valueNames.size(); i++) {
        auto memberView = getMembers<EnumView>()[i]->getViewIfDefined();
        sanityCheck(memberView, "One of the sequence members is undefined.");
        sanityEqualsCheck(i, memberView->value);
    }
}
