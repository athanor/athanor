
#ifndef SRC_OPERATORS_SIMPLEOPERATOR_H_
#define SRC_OPERATORS_SIMPLEOPERATOR_H_
#include "base/base.h"

template <typename View, typename Derived>
struct DefinedContainer {
    void setDefined(bool defined, bool trigger = false) {
        auto& op = static_cast<Derived&>(*this);
        bool triggerChange = trigger && (defined != op.isLocallyDefined());
        op.setLocallyDefined(defined);
        if (triggerChange) {
            if (defined) {
                op.reevaluate();
                visitTriggers([&](auto& t) { t->hasBecomeDefined(); },
                              op.triggers, true);
            } else {
                visitTriggers([&](auto& t) { t->hasBecomeUndefined(); },
                              op.triggers, true);
            }
        }
    }
    bool isDefined() { return static_cast<Derived&>(*this).isLocallyDefined(); }
    void copyDefinedStatus(DefinedContainer<View, Derived>&) const {}
    inline bool allOperandsAreDefined() { return isDefined(); }
};
template <typename Derived>
struct DefinedContainer<BoolView, Derived> {
    bool allOperandsDefined = false;
    void setDefined(bool defined, bool triggerChange = false) {
        auto& op = static_cast<Derived&>(*this);
        allOperandsDefined = defined;
        if (defined) {
            if (triggerChange) {
                op.changeValue([&]() {
                    op.reevaluate();
                    return true;
                });
            } else {
                op.reevaluate();
            }
        } else {
            if (triggerChange) {
                op.changeValue([&]() {
                    op.violation = LARGE_VIOLATION;
                    return true;
                });
            } else {
                op.violation = LARGE_VIOLATION;
            }
        }
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
    typedef typename OperatorTrates<Derived>::LeftTrigger LeftTrigger;
    typedef typename OperatorTrates<Derived>::RightTrigger RightTrigger;
    ExprRef<OperandView> left;
    ExprRef<OperandView> right;

    std::shared_ptr<LeftTrigger> leftTrigger;
    std::shared_ptr<RightTrigger> rightTrigger;

   public:
    SimpleBinaryOperator(ExprRef<OperandView> left, ExprRef<OperandView> right)
        : left(std::move(left)), right(std::move(right)) {
        invokeSetInnerType<View>();
    }

    template <typename V,
              typename std::enable_if<!std::is_same<BoolView, V>::value &&
                                          !std::is_same<IntView, V>::value,
                                      int>::type = 0>
    void invokeSetInnerType() {
        derived().setInnerType();
    }

    template <typename V,
              typename std::enable_if<std::is_same<BoolView, V>::value ||
                                          std::is_same<IntView, V>::value,
                                      int>::type = 0>
    void invokeSetInnerType() {
        // bool and int have no inner type, do nothing
    }
    auto& derived() { return *static_cast<Derived*>(this); }

    const auto& derived() const { return *static_cast<const Derived*>(this); }
    SimpleBinaryOperator(
        const SimpleBinaryOperator<View, OperandView, Derived>& other) = delete;
    SimpleBinaryOperator(
        SimpleBinaryOperator<View, OperandView, Derived>&& other)
        : left(std::move(other.left)),
          right(std::move(other.right)),
          leftTrigger(std::move(other.leftTrigger)),
          rightTrigger(std::move(other.rightTrigger)) {
        setTriggerParent(&derived(), leftTrigger, rightTrigger);
    }
    virtual ~SimpleBinaryOperator() { this->stopTriggeringOnChildren(); }

    void evaluateImpl() final;
    void reevaluate();
    void startTriggeringImpl() final;
    void stopTriggeringOnChildren();
    void stopTriggering() final;
    ExprRef<View> deepCopySelfForUnrollImpl(
        const ExprRef<View>&, const AnyIterRef& iterator) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction& func) final;
    bool optimiseImpl();
    std::pair<bool, ExprRef<View>> optimise(PathExtension path) final;
};

template <typename View, typename OperandView, typename Derived>
struct SimpleUnaryOperator : public View,
                             public DefinedContainer<View, Derived> {
    typedef typename OperatorTrates<Derived>::OperandTrigger OperandTrigger;
    ExprRef<OperandView> operand;

    std::shared_ptr<OperandTrigger> operandTrigger;

    SimpleUnaryOperator(ExprRef<OperandView> operand)
        : operand(std::move(operand)) {
        invokeSetInnerType<View>();
    }

    template <typename V,
              typename std::enable_if<!std::is_same<BoolView, V>::value &&
                                          !std::is_same<IntView, V>::value,
                                      int>::type = 0>
    void invokeSetInnerType() {
        derived().setInnerType();
    }

    template <typename V,
              typename std::enable_if<std::is_same<BoolView, V>::value ||
                                          std::is_same<IntView, V>::value,
                                      int>::type = 0>
    void invokeSetInnerType() {
        // bool and int have no inner type, do nothing
    }
    auto& derived() { return *static_cast<Derived*>(this); }
    const auto& derived() const { return *static_cast<const Derived*>(this); }
    SimpleUnaryOperator(
        const SimpleUnaryOperator<View, OperandView, Derived>& other) = delete;
    SimpleUnaryOperator(SimpleUnaryOperator<View, OperandView, Derived>&& other)
        : operand(std::move(other.operand)),
          operandTrigger(std::move(other.operandTrigger)) {
        setTriggerParent(&derived(), operandTrigger);
    }
    virtual ~SimpleUnaryOperator() { this->stopTriggeringOnChildren(); }
    void evaluateImpl() final;
    void reevaluate();
    void startTriggeringImpl() final;
    void stopTriggeringOnChildren();
    void stopTriggering() final;
    ExprRef<View> deepCopySelfForUnrollImpl(
        const ExprRef<View>&, const AnyIterRef& iterator) const final;
    void findAndReplaceSelf(const FindAndReplaceFunction& func) final;
    bool optimiseImpl();
    std::pair<bool, ExprRef<View>> optimise(PathExtension path) final;
};

#endif /* SRC_OPERATORS_SIMPLEOPERATOR_H_ */
