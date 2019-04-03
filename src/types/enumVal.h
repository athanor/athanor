
#ifndef SRC_TYPES_ENUMVAL_H_
#define SRC_TYPES_ENUMVAL_H_
#include <numeric>
#include <utility>
#include <vector>
#include "types/enum.h"
#include "utils/ignoreUnused.h"

struct EnumDomain {
    const std::string domainName;
    const std::vector<std::string> valueNames;
    EnumDomain(std::string domainName, std::vector<std::string> valueNames)
        : domainName(std::move(domainName)),
          valueNames(std::move(valueNames)) {}
};

struct EnumValue : public EnumView, ValBase {
    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<EnumView> deepCopyForUnrollImpl(
        const ExprRef<EnumView>&, const AnyIterRef& iterator) const final;

    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
    std::pair<bool, ExprRef<EnumView>> optimise(PathExtension) final;
    void debugSanityCheckImpl() const final;
    std::string getOpName() const final;
};

#endif /* SRC_TYPES_ENUMVAL_H_ */
