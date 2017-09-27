
#ifndef SRC_TYPES_PARENTCHECK_H_
#define SRC_TYPES_PARENTCHECK_H_

class ParentCheck {
    std::function<bool()> callBack;

   public:
    template <typename F>
    ParentCheck(F&& call) : callBack(std::forward<F>(call)) {
        static_assert(sizeof(F) <= 16,
                      "Error, parent check functions must be less than 16 "
                      "bytes so that it may be stack allocated.");
    }
    inline bool operator()() const { return callBack(); }
};

#endif /* SRC_TYPES_PARENTCHECK_H_ */
