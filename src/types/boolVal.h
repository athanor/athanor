#ifndef SRC_TYPES_BOOLVAL_H_
#define SRC_TYPES_BOOLVAL_H_
#include <utility>
#include <vector>

#include "types/bool.h"
struct BoolDomain {
    inline std::shared_ptr<BoolDomain> deepCopy(std::shared_ptr<BoolDomain>&) {
        return std::make_shared<BoolDomain>();
    }
    void merge(BoolDomain&) {}
};

struct BoolValue : public BoolView, ValBase {
    inline bool domainContainsValue(const BoolView&) { return true; }
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<BoolView> deepCopyForUnrollImpl(
        const ExprRef<BoolView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;

    void findAndReplaceSelf(const FindAndReplaceFunction&, PathExtension) final;
    std::pair<bool, ExprRef<BoolView>> optimiseImpl(ExprRef<BoolView>&,
                                                    PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
    void hashChecksImpl() const final;
};

#endif /* SRC_TYPES_BOOLVAL_H_ */
