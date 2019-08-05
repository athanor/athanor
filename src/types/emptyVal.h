#ifndef SRC_TYPES_EMPTYVAL_H_
#define SRC_TYPES_EMPTYVAL_H_
#include "base/base.h"
#include "common/common.h"
#include "types/empty.h"

struct EmptyDomain {
    inline std::shared_ptr<EmptyDomain> deepCopy(
        std::shared_ptr<EmptyDomain>&) {
        return std::make_shared<EmptyDomain>();
    }

    void merge(const EmptyDomain&) { shouldNotBeCalledPanic; }
};

struct EmptyValue : public EmptyView, public ValBase {
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<EmptyView> deepCopyForUnrollImpl(
        const ExprRef<EmptyView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<EmptyView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_EMPTYVAL_H_ */
