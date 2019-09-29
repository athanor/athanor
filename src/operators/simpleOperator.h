
#ifndef SRC_OPERATORS_SIMPLEOPERATOR_H_
#define SRC_OPERATORS_SIMPLEOPERATOR_H_
#include "base/base.h"

template <typename View, typename Derived>
struct DefinedContainer {
    inline void setDefined(bool defined) {
        auto& op = static_cast<Derived&>(*this);
        op.setAppearsDefined(defined);
    }
    inline void setUndefinedAndTrigger() {
        auto& op = static_cast<Derived&>(*this);
        bool wasDefined = isDefined();
        setDefined(false);
        if (wasDefined) {
            op.notifyValueUndefined();
        }
    }
    inline void setDefinedAndTrigger() {
        auto& op = static_cast<Derived&>(*this);
        bool wasDefined = isDefined();
        setDefined(true);
        if (!wasDefined) {
            op.notifyValueDefined();
        }
    }
    void reevaluateDefinedAndTrigger() {
        auto& op = static_cast<Derived&>(*this);
        if (!isDefined()) {
            op.reevaluate(true, true);
            if (isDefined()) {
                op.notifyValueDefined();
            }
        }
    }
    inline bool isDefined() {
        return static_cast<Derived&>(*this).appearsDefined();
    }
    void copyDefinedStatus(DefinedContainer<View, Derived>&) const {}
    inline bool allOperandsAreDefined() { return isDefined(); }
};
template <typename Derived>
struct DefinedContainer<BoolView, Derived> {
    bool allOperandsDefined = false;
    inline void setDefined(bool defined) {
        auto& op = static_cast<Derived&>(*this);
        allOperandsDefined = defined;
        if (!defined) {
            op.violation = LARGE_VIOLATION;
        }
    }
    inline void setUndefinedAndTrigger() {
        auto& op = static_cast<Derived&>(*this);
        op.changeValue([&]() {
            setDefined(false);
            return true;
        });
    }
    inline void setDefinedAndTrigger() {}
    inline void reevaluateDefinedAndTrigger() {
        auto& op = static_cast<Derived&>(*this);
        op.changeValue([&]() {
            op.reevaluate(true, true);
            return true;
        });
    }
    bool isDefined() { return true; }
    void copyDefinedStatus(DefinedContainer<BoolView, Derived>& other) const {
        other.allOperandsDefined = allOperandsDefined;
    }
    inline bool allOperandsAreDefined() { return allOperandsDefined; }
};

template <typename Derived>
struct OperatorTrates;
template <typename View, typename OperandView, typename Derived>
struct SimpleBinaryOperator : public View,
                              public DefinedContainer<View, Derived> {
    typedef SimpleBinaryOperator<View, OperandView, Derived> SimpleSuper;
    typedef typename OperatorTrates<Derived>::LeftTrigger LeftTrigger;
    typedef typename OperatorTrates<Derived>::RightTrigger RightTrigger;
    ExprRef<OperandView> left;
    ExprRef<OperandView> right;

    std::shared_ptr<LeftTrigger> leftTrigger;
    std::shared_ptr<RightTrigger> rightTrigger;

   public:
    SimpleBinaryOperator(ExprRef<OperandView> left, ExprRef<OperandView> right)
        : left(std::move(left)), right(std::move(right)) {}

    auto& derived() { return *static_cast<Derived*>(this); }

    const auto& derived() const { return *static_cast<const Derived*>(this); }
    SimpleBinaryOperator(
        const SimpleBinaryOperator<View, OperandView, Derived>& other) = delete;
    SimpleBinaryOperator(SimpleBinaryOperator<View, OperandView, Derived>&&) =
        delete;
    virtual ~SimpleBinaryOperator() { this->stopTriggeringOnChildren(); }

    void evaluateImpl() final;
    void reevaluate(bool leftChange, bool rightChange);
    void startTriggeringImpl() final;
    void stopTriggeringOnChildren();
    void stopTriggering() final;
    ExprRef<View> deepCopyForUnrollImpl(const ExprRef<View>&,
                                        const AnyIterRef& iterator) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction& func,
                            PathExtension path) final;
    std::pair<bool, std::shared_ptr<Derived>> standardOptimise(
        ExprRef<View>& self, PathExtension& path);
    std::pair<bool, ExprRef<View>> optimiseImpl(ExprRef<View>& self,
                                                PathExtension path) override;
    void standardSanityDefinednessChecks() const;
};

template <typename View, typename OperandView, typename Derived>
struct SimpleUnaryOperator : public View,
                             public DefinedContainer<View, Derived> {
    typedef SimpleUnaryOperator<View, OperandView, Derived> SimpleSuper;
    typedef typename OperatorTrates<Derived>::OperandTrigger OperandTrigger;
    ExprRef<OperandView> operand;

    std::shared_ptr<OperandTrigger> operandTrigger;

    SimpleUnaryOperator(ExprRef<OperandView> operand)
        : operand(std::move(operand)) {}

    auto& derived() { return *static_cast<Derived*>(this); }
    const auto& derived() const { return *static_cast<const Derived*>(this); }
    SimpleUnaryOperator(
        const SimpleUnaryOperator<View, OperandView, Derived>& other) = delete;
    SimpleUnaryOperator(SimpleUnaryOperator<View, OperandView, Derived>&&) =
        delete;
    virtual ~SimpleUnaryOperator() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void reevaluate(
        bool = false,
        bool = false);  // ignore bools, they are there to make it easier to
                        // compile between unary and binary ops

    void startTriggeringImpl() final;
    void stopTriggeringOnChildren();
    void stopTriggering() final;
    ExprRef<View> deepCopyForUnrollImpl(const ExprRef<View>&,
                                        const AnyIterRef& iterator) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction& func,
                            PathExtension path) final;
    std::pair<bool, std::shared_ptr<Derived>> standardOptimise(
        ExprRef<View>& self, PathExtension& path);
    std::pair<bool, ExprRef<View>> optimiseImpl(ExprRef<View>& self,
                                                PathExtension path) override;
    void standardSanityDefinednessChecks() const;
};

#endif /* SRC_OPERATORS_SIMPLEOPERATOR_H_ */
