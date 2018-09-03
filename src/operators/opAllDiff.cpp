#include "operators/opAllDiff.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include "operators/shiftViolatingIndices.h"
#include "operators/simpleOperator.hpp"
#include "utils/ignoreUnused.h"
using namespace std;
using OperandsSequenceTrigger =
    OperatorTrates<OpAllDiff>::OperandsSequenceTrigger;

OpAllDiff::OpAllDiff(OpAllDiff&& other)
    : SimpleUnaryOperator<BoolView, SequenceView, OpAllDiff>(move(other)),
      hashIndicesMap(move(other.hashIndicesMap)),
      indicesHashMap(move(other.indicesHashMap)),
      violatingOperands(move(other.violatingOperands)) {}
class OperatorTrates<OpAllDiff>::OperandsSequenceTrigger
    : public SequenceTrigger {
   public:
    OpAllDiff* op;
    OperandsSequenceTrigger(OpAllDiff* op) : op(op) {}

    void shiftIndices(UInt aboveIndex, bool up) {
        if (up) {
            for (size_t i = op->indicesHashMap.size(); i > aboveIndex; i--) {
                auto hash = op->indicesHashMap[i - 1];
                auto& indices = op->hashIndicesMap[hash];
                indices.erase(i - 1);
                indices.insert(i);
            }
            op->indicesHashMap.insert(op->indicesHashMap.begin() + aboveIndex,
                                      0);

        } else {
            for (size_t i = aboveIndex + 1; i < op->indicesHashMap.size();
                 i++) {
                auto hash = op->indicesHashMap[i];
                auto& indices = op->hashIndicesMap[hash];
                indices.erase(i);
                indices.insert(i - 1);
            }
            op->indicesHashMap.erase(op->indicesHashMap.begin() + aboveIndex);
        }
    }

    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        if (!op->allOperandsAreDefined()) {
            return;
        }
        shiftIndices(index, true);
        shiftIndicesUp(index, op->operand->view().numberElements(),
                       op->violatingOperands);

        HashType newMemberHash = getValueHash(exprIn);
        if (op->addHash(newMemberHash, index) > 1) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
        debug_code(op->assertValidState());
    }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        if (!op->allOperandsAreDefined()) {
            return;
        }

        HashType hash = getValueHash(exprIn);
        if (op->removeHash(hash, index) >= 1) {
            op->changeValue([&]() {
                --op->violation;
                return true;
            });
        }
        shiftIndices(index, false);
        shiftIndicesDown(index, op->operand->view().numberElements(),
                         op->violatingOperands);

        debug_code(op->assertValidState());
    }

    inline void positionsSwapped(UInt index1, UInt index2) {
        if (op->violatingOperands.count(index1)) {
            if (!op->violatingOperands.count(index2)) {
                op->violatingOperands.erase(index1);
                op->violatingOperands.insert(index2);
            }
        } else {
            if (op->violatingOperands.count(index2)) {
                op->violatingOperands.erase(index2);
                op->violatingOperands.insert(index1);
            }
        }

        auto hash1 = op->indicesHashMap[index1];
        auto hash2 = op->indicesHashMap[index2];
        swap(op->indicesHashMap[index1], op->indicesHashMap[index2]);
        auto& indices1 = op->hashIndicesMap[hash1];
        auto& indices2 = op->hashIndicesMap[hash2];
        indices1.erase(index1);
        indices2.erase(index2);
        indices1.insert(index2);
        indices2.insert(index1);
        debug_code(op->assertValidState());
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        if (!op->allOperandsAreDefined()) {
            return;
        }
        Int violationDelta = 0;
        mpark::visit(
            [&](auto& members) {
                for (size_t i = startIndex; i < endIndex; i++) {
                    HashType oldHash = op->indicesHashMap[i];
                    HashType newHash = getValueHash(members[i]->view());
                    if (op->removeHash(oldHash, i) >= 1) {
                        --violationDelta;
                    }
                    if (op->addHash(newHash, i) > 1) {
                        ++violationDelta;
                    }
                }
            },
            op->operand->view().members);
        op->changeValue([&]() {
            op->violation += violationDelta;
            return true;
        });
        debug_code(op->assertValidState());
    }

    void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate();
            return true;
        });
    }

    void reattachTrigger() final {
        deleteTrigger(op->operandTrigger);
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }

    void hasBecomeUndefined() final { op->setDefined(false, true); }
    void hasBecomeDefined() final {
        op->reevaluate();
        op->setDefined(true, true);
    }
    void memberHasBecomeUndefined(UInt) final {
        if (op->operand->view().numberUndefined == 1) {
            op->setDefined(false, true);
        }
    }
    void memberHasBecomeDefined(UInt) final {
        if (op->operand->view().numberUndefined == 0) {
            op->reevaluate();
            op->setDefined(true, true);
        }
    }
};

void OpAllDiff::reevaluate() {
    violation = 0;
    hashIndicesMap.clear();
    indicesHashMap.clear();
    mpark::visit(
        [&](auto& members) {
            indicesHashMap.resize(members.size());
            for (size_t i = 0; i < members.size(); i++) {
                auto& member = members[i];
                if (member->isUndefined()) {
                    return;
                }
                HashType hash = getValueHash(member->view());
                if (addHash(hash, i) > 1) {
                    ++violation;
                }
            }
        },
        operand->view().members);
    debug_code(assertValidState());
}

void OpAllDiff::updateVarViolationsImpl(const ViolationContext&,
                                        ViolationContainer& vioContainer) {
    mpark::visit(
        [&](auto& members) {
            for (size_t index : violatingOperands) {
                members[index]->updateVarViolations(
                    hashIndicesMap[indicesHashMap[index]].size(), vioContainer);
            }
        },
        operand->view().members);
}

void OpAllDiff::copy(OpAllDiff& newOp) const {
    newOp.violation = violation;
    newOp.hashIndicesMap = hashIndicesMap;
    newOp.indicesHashMap = indicesHashMap;
    newOp.violatingOperands = violatingOperands;
}

std::ostream& OpAllDiff::dumpState(std::ostream& os) const {
    os << "OpAllDiff: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                         violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
    return operand->dumpState(os) << ")";
}

bool OpAllDiff::optimiseImpl() { return false; }

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAllDiff> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};

ExprRef<BoolView> OpMaker<OpAllDiff>::make(ExprRef<SequenceView> o) {
    return make_shared<OpAllDiff>(move(o));
}

void OpAllDiff::assertValidState() {
    bool success = true;
    for (size_t i = 0; i < indicesHashMap.size(); i++) {
        HashType hash = indicesHashMap[i];
        auto iter = hashIndicesMap.find(hash);
        if (iter == hashIndicesMap.end()) {
            cerr << "Error: hash " << hash << " at index " << i
                 << " mapped to no set in hashIndicesMap\n";
            success = false;
            break;
        }
        auto& indices = iter->second;
        if (!indices.count(i)) {
            cerr
                << "Error: index " << i << " maps to hash "
                << " hash "
                << " but hashIndicesMap does not have the hash index mapping\n";
            cerr << "Error: Hash " << hash << " maps to " << indices << endl;
            success = false;
            break;
        }
        if (indices.size() > 2) {
        }
    }
    size_t total = 0;
    vector<Int> duplicateOperands;
    if (success) {
        for (auto& hashIndicesPair : hashIndicesMap) {
            total += hashIndicesPair.second.size();
            if (hashIndicesPair.second.size() > 1) {
                for (const auto& index : hashIndicesPair.second) {
                    duplicateOperands.push_back(index);
                }
            }
        }
        if (total != indicesHashMap.size()) {
            cerr << "Error: found " << total
                 << " indices in hashIndicesMap but indicesHashMap has a size "
                    "of "
                 << indicesHashMap.size() << endl;
            success = false;
        }
    }
    if (success) {
        for (auto& index : duplicateOperands) {
            if (!violatingOperands.count(index)) {
                cerr << "Error: violatingOperands does not contain index "
                     << index << endl;
                success = false;
                break;
            }
        }
    }
    if (success && duplicateOperands.size() != violatingOperands.size()) {
        cerr << "Error: violatingOperands has too many indices.\n";
        success = false;
    }
    if (!success) {
        cerr << "hashIndicesMap: " << hashIndicesMap << endl;
        cerr << "indicesHashMap: " << indicesHashMap << endl;
        vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                             violatingOperands.end());
        sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());

        cerr << "violatingOperands(sorted): " << sortedViolatingOperands
             << endl;
        assert(false);
        abort();
    }
}

template struct SimpleUnaryOperator<BoolView, SequenceView, OpAllDiff>;
