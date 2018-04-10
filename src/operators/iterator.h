
#ifndef SRC_OPERATORS_ITERATOR_H_
#define SRC_OPERATORS_ITERATOR_H_

#include "base/base.h"
#include "base/exprRef.h"
template <typename View>
struct Iterator : public ExprInterface<View> {
    typedef typename AssociatedTriggerType<View>::type TriggerType;
    std::vector<std::shared_ptr<TriggerBase>> triggers;
    u_int64_t id;
    ExprRef<View> ref;

    Iterator(u_int64_t id, ExprRef<View> ref) : id(id), ref(std::move(ref)) {}
    template <typename Func>
    inline void changeValue(bool triggering, const ExprRef<View>& oldVal,
                            const ExprRef<View>& newVal, Func&& callback) {
        ref = newVal;
        callback();
        if (triggering) {
            ref = oldVal;
            visitTriggers([&](auto& t) { t->possibleValueChange(); }, triggers);
            ref = newVal;
            visitTriggers([&](auto& t) { t->valueChanged(); }, triggers);
        }
    }
    inline ExprRef<View>& getValue() { return ref; }

    Iterator(const Iterator<View>&) = delete;
    Iterator(Iterator<View>&& other);
    ~Iterator() { this->stopTriggeringOnChildren(); }
    void addTrigger(const std::shared_ptr<TriggerType>& trigger) final;
    View& view() final;
    const View& view() const final;

    void evaluate() final;
    void startTriggering() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren() {}

    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<View> deepCopySelfForUnroll(const ExprRef<View>& self,
                                        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

template <typename T>
using IterRef = std::shared_ptr<Iterator<T>>;
template <typename Value>
using IterRefMaker = IterRef<typename AssociatedViewType<Value>::type>;

struct AnyIterRef : public Variantised<IterRefMaker> {
    using Variantised<IterRefMaker>::variant;
    inline auto& asVariant() {
        return static_cast<Variantised<IterRefMaker>&>(*this);
    }
    inline const auto& asVariant() const {
        return static_cast<const Variantised<IterRefMaker>&>(*this);
    }
};
std::ostream& operator<<(std::ostream& os, const AnyIterRef& iter);
#endif /* SRC_OPERATORS_ITERATOR_H_ */
