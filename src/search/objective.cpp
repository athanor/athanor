#include "search/objective.h"
#include "types/int.h"
#include "types/tuple.h"
using namespace std;
static const char* NO_OBJ_UNDEFINED =
    "Error: trying to build an objective from an undefined value.\n";
Objective::Objective(OptimiseMode mode, const ExprRef<IntView>& expr)
    : mode(mode),
      value(expr->getViewIfDefined().checkedGet(NO_OBJ_UNDEFINED).value) {}

template <typename Array>
void assignElements(const TupleView& tuple, Array& array) {
    for (size_t i = 0; i < tuple.members.size(); i++) {
        Int memberValue = mpark::get<ExprRef<IntView>>(tuple.members[i])
                              ->getViewIfDefined()
                              .checkedGet(NO_OBJ_UNDEFINED)
                              .value;
        array[i] = memberValue;
    }
}
template <unsigned long i>
inline void assign(TupleView& tuple, std::array<Int, i>& array) {
    array = {};
    assignElements(tuple, array);
}

Objective::Objective(OptimiseMode mode, const ExprRef<TupleView>& expr)
    : mode(mode) {
    auto& tupleView = expr->getViewIfDefined().checkedGet(NO_OBJ_UNDEFINED);
    switch (tupleView.members.size()) {
        case 1: {
            assign(tupleView, value.emplace<std::array<Int, 1>>());
            break;
        }
        case 2: {
            assign(tupleView, value.emplace<std::array<Int, 2>>());
            break;
        }
        case 3: {
            assign(tupleView, value.emplace<std::array<Int, 3>>());
            break;
        }
        case 0:
            cerr << "Error, cannot build objective value from empty tuple\n";
            abort();
        default: {
            assignElements(tupleView, value.emplace<vector<Int>>(
                                          tupleView.members.size(), 0));
            break;
        }
    }
}

bool Objective::operator<(const Objective& other) const {
    debug_code(assert(mode == other.mode));
    return mpark::visit(
        [&](const auto& value) {
            const auto& otherValue =
                mpark::get<BaseType<decltype(value)>>(other.value);
            return (mode == OptimiseMode::MINIMISE) ? value < otherValue
                                                    : otherValue < value;
        },
        value);
}

bool Objective::operator<=(const Objective& other) const {
    debug_code(assert(mode == other.mode));
    return mpark::visit(
        [&](const auto& value) {
            const auto& otherValue =
                mpark::get<BaseType<decltype(value)>>(other.value);
            return (mode == OptimiseMode::MINIMISE) ? value <= otherValue
                                                    : otherValue <= value;
        },
        value);
}

bool Objective::operator==(const Objective& other) const {
    debug_code(assert(mode == other.mode));
    return mpark::visit(
        [&](const auto& value) {
            const auto& otherValue =
                mpark::get<BaseType<decltype(value)>>(other.value);
            return value == otherValue;
        },
        value);
}

ostream& operator<<(ostream& os, const Objective& o) {
    mpark::visit(overloaded([&](Int value) { os << value; },
                            [&](const auto& value) {
                                os << "(";
                                bool first = true;
                                for (const auto& i : value) {
                                    if (first) {
                                        first = false;
                                    } else {
                                        os << ",";
                                    }
                                    os << i;
                                }
                                os << ")";
                            }),
                 o.value);
    return os;
}
