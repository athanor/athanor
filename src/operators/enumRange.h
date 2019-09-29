
#ifndef SRC_OPERATORS_ENUMRANGE_H_
#define SRC_OPERATORS_ENUMRANGE_H_
#include "types/enumVal.h"
#include "types/sequence.h"

struct EnumRange : public SequenceView {
    std::shared_ptr<EnumDomain> domain;

    EnumRange(std::shared_ptr<EnumDomain> domain) : domain(std::move(domain)) {
        this->members.emplace<ExprRefVec<EnumView>>();
    }
    EnumRange(EnumRange&&) = delete;
    EnumRange(const EnumRange&) = delete;
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<SequenceView> deepCopyForUnrollImpl(
        const ExprRef<SequenceView>&, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<SequenceView>> optimiseImpl(
        ExprRef<SequenceView>&, PathExtension path) final;
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
};
#endif /* SRC_OPERATORS_ENUMRANGE_H_ */
