
#ifndef SRC_OPERATORS_FLATTEN_H_
#define SRC_OPERATORS_FLATTEN_H_
#include "operators/opSequenceLit.h"
#include "utils/ignoreUnused.h"
template <typename Op, typename View>
bool replaceIfOperandIsSameAsOp(Op& op, ExprRefVec<View>& members,
                                size_t index) {
    ignoreUnused(op);
    auto member = members[index];  // copying the shared ptr
    auto sameOpTest = getAs<Op>(member);
    if (sameOpTest) {
        auto opSequenceLitTest = getAs<OpSequenceLit>(sameOpTest->operand);
        if (opSequenceLitTest) {
            auto& operandMembers =
                lib::get<ExprRefVec<View>>(opSequenceLitTest->members);
            debug_log("Optimise: flattening: lifting operands for op "
                      << op.getOpName());
            members[index] = std::move(members.back());
            members.pop_back();
            for (auto& operandMember : operandMembers) {
                members.emplace_back(std::move(operandMember));
            }
            return true;
        }
    }
    return false;
}

template <typename OpViewType, typename Op>
bool flatten(Op& op) {
    auto opSequenceLitTest = getAs<OpSequenceLit>(op.operand);
    if (!opSequenceLitTest) {
        return false;
    }
    ExprRefVec<OpViewType>* membersTest =
        lib::get_if<ExprRefVec<OpViewType>>(&(opSequenceLitTest->members));
    if (!membersTest) {
        return false;
    }
    auto& members = *membersTest;
    bool optimised = false;
    for (size_t i = 0; i < members.size(); i++) {
        optimised |= replaceIfOperandIsSameAsOp(op, members, i);
    }
    return optimised;
}

#endif /* SRC_OPERATORS_FLATTEN_H_ */
