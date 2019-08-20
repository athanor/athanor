
#ifndef SRC_TYPES_ENUMVAL_H_
#define SRC_TYPES_ENUMVAL_H_
#include <numeric>
#include <utility>
#include <vector>
#include "types/enum.h"
#include "utils/ignoreUnused.h"

struct EnumDomain {
   public:
    const std::string domainName;

   private:
    const std::vector<std::string> valueNames;
    const size_t numberUnnamed =
        0;  // if not 0, means this enum is an unnamed type

    EnumDomain(std::string domainName, size_t numberUnnamed)
        : domainName(std::move(domainName)), numberUnnamed(numberUnnamed) {}

   public:
    EnumDomain(std::string domainName, std::vector<std::string> valueNames)
        : domainName(std::move(domainName)),
          valueNames(std::move(valueNames)) {}

    static inline std::shared_ptr<EnumDomain> UnnamedType(
        std::string typeName, size_t numberUnnamed) {
        return std::make_shared<EnumDomain>(
            EnumDomain(std::move(typeName), numberUnnamed));
    }

    inline std::string name(size_t i) const {
        debug_code(assert(i < numberValues()));
        return (numberUnnamed != 0) ? toString(domainName, "_", i)
                                    : valueNames[i];
    }

    void printValue(std::ostream& os, size_t i) const {
        debug_code(assert(i < numberValues()));
        if (numberUnnamed != 0) {
            os << domainName << "_" << i;
        } else {
            os << valueNames[i];
        }
    }

    inline size_t numberValues() const {
        return (numberUnnamed == 0) ? valueNames.size() : numberUnnamed;
    }
    inline std::shared_ptr<EnumDomain> deepCopy(
        std::shared_ptr<EnumDomain>& self) {
        return self;
    }
    void merge(const EnumDomain&) {}
};

struct EnumValue : public EnumView, ValBase {
    inline bool domainContainsValue(const EnumView&) { return true; }
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
