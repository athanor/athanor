
#ifndef SRC_OPERATORS_ITERATOR_H_
#define SRC_OPERATORS_ITERATOR_H_

#include "base/base.h"
#include "base/exprRef.h"
template <typename View>
struct Iterator : public ExprInterface<View> {
    typedef typename AssociatedTriggerType<View>::type TriggerType;
    struct RefTrigger;
    std::vector<std::shared_ptr<TriggerBase>> triggers;
    u_int64_t id;
    ExprRef<View> ref;
    std::shared_ptr<RefTrigger> refTrigger;
    Iterator(u_int64_t id, ExprRef<View> ref) : id(id), ref(std::move(ref)) {}
    template <typename Func>
    inline void changeValue(bool triggering, const ExprRef<View>& oldVal,
                            const ExprRef<View>& newVal, Func&& callback) {
        this->setAppearsDefined(true);
        if (!triggering) {
            ref = newVal;
            callback();
        } else {
            ref = oldVal;
            callback();
            bool oldValUndefined = oldVal->isUndefined();
            ref = newVal;
            bool newValUndefined = newVal->isUndefined();
            if (oldValUndefined && !newValUndefined) {
                visitTriggers([&](auto& t) { t->hasBecomeDefined(); }, triggers,
                              true);
            } else if (!oldValUndefined && newValUndefined) {
                visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                              triggers, true);
            } else if (!newValUndefined) {
                visitTriggers(
                    [&](auto t) {
                        t->valueChanged();
                        t->reattachTrigger();
                    },
                    triggers);
            }
        }
    }

    inline ExprRef<View>& getValue() { return ref; }

    Iterator(const Iterator<View>&) = delete;
    Iterator(Iterator<View>&& other);
    ~Iterator() { this->stopTriggeringOnChildren(); }
    void addTriggerImpl(const std::shared_ptr<TriggerType>& trigger,
                        bool includeMembers, Int memberIndex) final;
    OptionalRef<View> view() final;
    OptionalRef<const View> view() const final;

    void evaluateImpl() final;
    void startTriggeringImpl() final;
    void stopTriggering() final;
    void stopTriggeringOnChildren();

    void updateVarViolationsImpl(const ViolationContext& vioContext,
                                 ViolationContainer&) final;
    ExprRef<View> deepCopySelfForUnrollImpl(
        const ExprRef<View>& self, const AnyIterRef& iterator) const final;
    std::ostream& dumpState(std::ostream& os) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction&) final;

    std::pair<bool, ExprRef<View>> optimise(PathExtension path) final;
    template <typename IterView = View,
              typename std::enable_if<std::is_same<BoolView, IterView>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool) {}
    template <typename IterView = View,
              typename std::enable_if<!std::is_same<BoolView, IterView>::value,
                                      int>::type = 0>
    void setAppearsDefined(bool set) {
        Undefinable<IterView>::setAppearsDefined(set);
    }
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
