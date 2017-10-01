#ifndef SRC_UTILS_STACKFUNCTION_H_
#define SRC_UTILS_STACKFUNCTION_H_
#include <functional>
template <typename FunctionType>
class StackFunction {
    std::function<FunctionType> callBack;

   public:
    template <typename F>
    StackFunction(F&& call) : callBack(std::forward<F>(call)) {
        static_assert(sizeof(F) <= 16,
                      "Error, StackFunction is based on std::function which "
                      "needs to have a size  <= 16 "
                      "bytes if it is to be stack allocated.");
    }
    template <typename... Args>
    inline auto operator()(Args&&... args) const
        -> decltype(callBack(std::forward<Args>(args)...)) {
        return callBack(std::forward<Args>(args)...);
    }
};

#endif /* SRC_UTILS_STACKFUNCTION_H_ */
