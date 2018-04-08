
#ifndef SRC_OPERATORS_ITERATOR_H_
#define SRC_OPERATORS_ITERATOR_H_

#include "base/base.h"
#include "base/exprRef.h"
template <typename View>
struct Iterator : public ExprInterface<View> {
    typedef typename AssociatedTriggerType<View>::type TriggerType;
    std::vector<std::shared_ptr<TriggerType>> triggers;

    u_int64_t id;
    ExprRef<View> ref;
    std::shared_ptr<ForwardingTriggerBase<TriggerType>> refTrigger;

    Iterator(u_int64_t id, ExprRef<View> ref) : id(id), ref(std::move(ref)) {}
    template <typename Func>
    inline void changeValue(bool triggering, ExprRef<View>& newVal,
                            Func&& callback) {
        std::swap(ref, newVal);
        callback();
        if (triggering) {
            std::swap(ref, newVal);
            visitTriggers([&](auto& t) { t->possibleValueChange(); }, triggers);
            std::swap(ref, newVal);
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
    void stopTriggeringOnChildren();
    void updateViolationDescription(UInt parentViolation,
                                    ViolationDescription&) final;
    ExprRef<View> deepCopySelfForUnroll(ExprRef<View>& self,
                                        const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;
};

template <typename T>
using IterRef = std::shared_ptr<Iterator<T>>;
template <typename Value>
using IterRefMaker = IterRef<typename AssociatedViewType<Value>::type>;

struct AnyIterRef : public Variantised<IterRefMaker> {
    inline auto& asVariant() {
        return static_cast<Variantised<IterRefMaker>&>(*this);
    }
    inline const auto& asVariant() const {
        return static_cast<const Variantised<IterRefMaker>&>(*this);
    }
};
#endif /* SRC_OPERATORS_ITERATOR_H_ */
