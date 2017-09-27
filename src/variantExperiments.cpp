#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "utils/variantOperations.h"
using var_t = mpark::variant<double, int, std::string, long>;
void variantExperiments() {
    std::vector<var_t> vec = {10, 15l, 1.5, "hello"};
    for (auto& v : vec) {
        // void visitor, only called for side-effects
        mpark::visit([](auto&& arg) { std::cout << arg; }, v);

        // value-returning visitor. A common idiom is to return another
        var_t w =
            mpark::visit([](auto&& arg) -> var_t { return arg + arg; }, v);

        std::cout << ". After doubling, variant holds ";
        // type-matching visitor: can also be a class with 4 overloaded
        // operator()'s
        mpark::visit(
            [](auto&& arg) {
                if
                    variantMatch(int, arg) {
                        std::cout << "int with value " << arg << '\n';
                    }
                else if
                    variantMatch(long, arg) {
                        std::cout << "long with value " << arg << '\n';
                    }
                else if
                    variantMatch(double, arg) {
                        std::cout << "double with value " << arg << '\n';
                    }
                else if
                    variantMatch(std::string, arg) {
                        std::cout << "std::string with value "
                                  << std::quoted(arg) << '\n';
                    }
                else {
                    nonExhaustiveError(arg);
                }
            },
            w);
    }

    for (auto& v : vec) {
        mpark::visit(overloaded([](auto arg) { std::cout << arg << ' '; },
                                [](double arg) {
                                    std::cout << std::fixed << arg << ' ';
                                },
                                [](const std::string& arg) {
                                    std::cout << std::quoted(arg) << ' ';
                                }),
                     v);
    }
}
