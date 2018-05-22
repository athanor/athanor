
#ifndef SRC_OPERATORS_FLATTEN_H_
#define SRC_OPERATORS_FLATTEN_H_
#include "operators/opSequenceLit.h"

template <typename Op, typename View>
bool replaceIfOperandIsSameAsOp(ExprRefVec<View>& members, size_t index);
template <typename OpViewType, typename Op>
bool flatten(Op& op) {
    OpSequenceLit* opSequenceLitTest =
        dynamic_cast<OpSequenceLit*>(&(*op.operand));
    if (!opSequenceLitTest) {
        return false;
    }
    ExprRefVec<OpViewType>* membersTest =
        mpark::get_if<ExprRefVec<OpViewType>>(&(opSequenceLitTest->members));
    if (!membersTest) {
        return false;
    }
    auto& members = *membersTest;
    bool changeMade = false;
    for (size_t i = 0; i < members.size(); i++) {
        changeMade |= replaceIfOperandIsSameAsOp<Op>(members, i);
    }
    return changeMade;
}

template <typename Op, typename View>
bool replaceIfOperandIsSameAsOp(ExprRefVec<View>& members, size_t index) {
    auto member = members[index];  // copying the shared ptr
    Op* sameOpTest = dynamic_cast<Op*>(&(*member));
    if (sameOpTest) {
        OpSequenceLit* opSequenceLitTest =
            dynamic_cast<OpSequenceLit*>(&(*sameOpTest->operand));
        if (opSequenceLitTest) {
            auto& operandMembers =
                mpark::get<ExprRefVec<View>>(opSequenceLitTest->members);
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

#endif /* SRC_OPERATORS_FLATTEN_H_ */
