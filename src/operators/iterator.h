#ifndef SRC_OPERATORS_ITERATOR_H_
#define SRC_OPERATORS_ITERATOR_H_

#include "base/base.h"
#include "triggers/allTriggers.h"
template <typename View>
struct Iterator : public ExprInterface<View>, public TriggerContainer<View> {
    typedef typename AssociatedTriggerType<View>::type TriggerType;
    struct RefTrigger;
    u_int64_t id;
    ExprRef<View> ref;
    std::shared_ptr<RefTrigger> refTrigger;
    Iterator(u_int64_t id, ExprRef<View> ref) : id(id), ref(std::move(ref)) {}
    void reattachRefTrigger();

    void changeValueSilent(const ExprRef<View>& newVal) {
        ref = newVal;
        this->setAppearsDefined(ref->appearsDefined());
    }

    template <typename Func>
    inline void changeValue(bool triggering, ExprRef<View>& newVal,
                            Func&& callback) {
        if (!triggering) {
            ref = newVal;
            callback();
            this->setAppearsDefined(ref->appearsDefined());
        } else {
            // slight hack, ref currently has old val.  Temp assign it to new
            // new val and tell iterator to trigger off of new val then reassign
            // iter ator back to old val.  This means that when startTriggering
            // is called on the iterator, it will not bother triggering on the
            // old val as it is already triggering on something (the new val).
            // The old val needs to be there as parent expressions will expect
            // it to be so when startTriggering is called.
            std::swap(ref, newVal);
            reattachRefTrigger();
            std::swap(ref, newVal);
            callback();
            bool oldValUndefined = !ref->appearsDefined();
            ref = newVal;
            bool newValUndefined = !newVal->appearsDefined();
            this->setAppearsDefined(!newValUndefined);
            if (oldValUndefined && !newValUndefined) {
                this->notifyValueDefined();
            } else if (!oldValUndefined && newValUndefined) {
                this->notifyValueUndefined();
            } else if (!newValUndefined) {
                this->notifyEntireValueChanged();
            }
        }
    }

    inline ExprRef<View>& getValue() { return ref; }

    Iterator(const Iterator<View>&) = delete;
    Iterator(Iterator<View>&&) = delete;
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
    ExprRef<View> deepCopyForUnrollImpl(const ExprRef<View>& self,
                                        const AnyIterRef& iterator) const final;
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
    std::string getOpName() const final;
    void debugSanityCheckImpl() const final;
    inline bool allowForwardingOfTrigger() { return true; }
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
