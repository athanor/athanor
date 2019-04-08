#ifndef SRC_SEARCH_OBJECTIVE_H_
#define SRC_SEARCH_OBJECTIVE_H_
#include <algorithm>
#include <cassert>
#include "base/base.h"
#include "common/common.h"
enum class OptimiseMode { NONE, MAXIMISE, MINIMISE };

class Objective {
    // objective can be int or essence tuple of int (for lex ordering).
    // In tuple case, if 3 or less, use stack allocated array, otherwise use
    // vector
    OptimiseMode mode;
    typedef mpark::variant<Int, std::array<Int, 1>, std::array<Int, 2>,
                           std::array<Int, 3>, std::vector<Int>>
        ObjType;
    ObjType value = 0;

   public:
    Objective(OptimiseMode mode, const ExprRef<IntView>& exprValue);
    Objective(OptimiseMode mode, const ExprRef<TupleView>& exprValue);

    bool operator==(const Objective& other) const;
    bool operator<(const Objective& other) const;
    bool operator<=(const Objective& other) const;
    friend std::ostream& operator<<(std::ostream& os, const Objective& obj);
};

#endif /* SRC_SEARCH_OBJECTIVE_H_ */
