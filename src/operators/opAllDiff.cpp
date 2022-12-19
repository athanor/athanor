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
                                      HashType(0));

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

    pair<bool, HashType> getHashIfDefined(const AnyExprRef& expr) {
        return lib::visit(
            [&](auto& expr) -> pair<bool, HashType> {
                auto view = expr->getViewIfDefined();
                if (!view) {
                    return make_pair(false, HashType(0));
                }
                return make_pair(true, getValueHash(*view));
            },
            expr);
    }
    void valueAdded(UInt index, const AnyExprRef& exprIn) final {
        shiftIndices(index, true);
        shiftIndicesUp(index, op->operand->view()->numberElements(),
                       op->violatingOperands);
        auto boolHashPair = getHashIfDefined(exprIn);
        if (!boolHashPair.first) {
            memberHasBecomeUndefined(index);
            return;
        }

        if (op->addHash(boolHashPair.second, index) > 1) {
            op->changeValue([&]() {
                ++op->violation;
                return true;
            });
        }
        debug_code(op->assertValidState());
    }

    void valueRemoved(UInt index, const AnyExprRef& exprIn) final {
        if (appearsDefined(exprIn)) {
            if (op->removeHash(op->indicesHashMap[index], index) >= 1) {
                op->changeValue([&]() {
                    --op->violation;
                    return true;
                });
            }
        } else if (op->operand->view()->numberUndefined == 0) {
            op->reevaluateDefinedAndTrigger();
            return;
        }

        shiftIndices(index, false);
        shiftIndicesDown(index, op->operand->view()->numberElements(),
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

    void memberReplaced(UInt index, const AnyExprRef&) final {
        subsequenceChanged(index, index + 1);
    }
    inline void subsequenceChanged(UInt startIndex, UInt endIndex) final {
        if (!op->allOperandsAreDefined()) {
            return;
        }
        Int violationDelta = 0;
        bool foundUndefined = false;
        lib::visit(
            [&](auto& members) {
                for (size_t i = startIndex; i < endIndex; i++) {
                    auto memberView = members[i]->getViewIfDefined();
                    if (!memberView) {
                        // value has become undefined, that trigger will
                        // eventually reach this op, ignore this event
                        foundUndefined = true;
                        return;
                    }
                    HashType oldHash = op->indicesHashMap[i];
                    HashType newHash = getValueHash(*memberView);
                    if (op->removeHash(oldHash, i) >= 1) {
                        --violationDelta;
                    }
                    if (op->addHash(newHash, i) > 1) {
                        ++violationDelta;
                    }
                }
            },
            op->operand->view()->members);
        if (foundUndefined) {
            return;
        }
        op->changeValue([&]() {
            op->violation += violationDelta;
            return true;
        });
        debug_code(op->assertValidState());
    }

    void valueChanged() final {
        op->changeValue([&]() {
            op->reevaluate();
            return op->allOperandsAreDefined();
        });
    }

    void reattachTrigger() final {
        auto trigger = make_shared<OperandsSequenceTrigger>(op);
        op->operand->addTrigger(trigger);
        op->operandTrigger = trigger;
    }

    void hasBecomeUndefined() final { op->setUndefinedAndTrigger(); }
    void hasBecomeDefined() final { op->reevaluateDefinedAndTrigger(); }
    void memberHasBecomeUndefined(UInt) final {
        if (op->operand->view()->numberUndefined == 1) {
            op->setUndefinedAndTrigger();
        }
    }
    void memberHasBecomeDefined(UInt) final {
        if (op->operand->view()->numberUndefined == 0) {
            op->reevaluate();
            op->reevaluateDefinedAndTrigger();
        }
    }
};

void OpAllDiff::reevaluateImpl(SequenceView& operandView) {
    if (operandView.numberUndefined > 0) {
        setDefined(false);
        return;
    }
    violation = 0;
    hashIndicesMap.clear();
    indicesHashMap.clear();
    lib::visit(
        [&](auto& members) {
            indicesHashMap.resize(members.size());
            for (size_t i = 0; i < members.size(); i++) {
                auto& member = members[i];
                auto memberView = member->getViewIfDefined();
                if (!memberView) {
                    setDefined(false);
                    return;
                }
                HashType hash = getValueHash(*memberView);
                if (addHash(hash, i) > 1) {
                    ++violation;
                }
            }
        },
        operandView.members);
    debug_code(assertValidState());
}

void OpAllDiff::updateVarViolationsImpl(const ViolationContext& vioContext,
                                        ViolationContainer& vioContainer) {
    auto* boolVioContextTest =
        dynamic_cast<const BoolViolationContext*>(&vioContext);
    lib::visit(
        [&](auto& members) {
            if (boolVioContextTest && boolVioContextTest->negated) {
                if (violation == 0) {
                    for (auto& member : members) {
                        member->updateVarViolations(1, vioContainer);
                    }
                }
                return;
            }
            for (size_t index : violatingOperands) {
                members[index]->updateVarViolations(
                    hashIndicesMap[indicesHashMap[index]].size(), vioContainer);
            }
        },
        operand->view()->members);
}

void OpAllDiff::copy(OpAllDiff&) const {}

std::ostream& OpAllDiff::dumpState(std::ostream& os) const {
    os << "OpAllDiff: violation=" << violation << endl;
    vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                         violatingOperands.end());
    sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());
    os << "Violating indices: " << sortedViolatingOperands << endl;
    return operand->dumpState(os) << ")";
}

void OpAllDiff::assertValidState() {
    bool success = true;
    for (size_t i = 0; i < indicesHashMap.size(); i++) {
        HashType hash = indicesHashMap[i];
        auto iter = hashIndicesMap.find(hash);
        if (iter == hashIndicesMap.end()) {
            myCerr << "Error: hash " << hash << " at index " << i
                   << " mapped to no set in hashIndicesMap\n";
            success = false;
            break;
        }
        auto& indices = iter->second;
        if (!indices.count(i)) {
            myCerr << "Error: index " << i << " maps to hash "
                   << " hash "
                   << " but hashIndicesMap does not have the hash index "
                      "mapping\n";
            myCerr << "Error: Hash " << hash << " maps to " << indices << endl;
            success = false;
            break;
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
            myCerr << "Error: found " << total
                   << " indices in hashIndicesMap but indicesHashMap has a "
                      "size "
                      "of "
                   << indicesHashMap.size() << endl;
            success = false;
        }
    }
    if (success) {
        for (auto& index : duplicateOperands) {
            if (!violatingOperands.count(index)) {
                myCerr << "Error: violatingOperands does not contain index "
                       << index << endl;
                success = false;
                break;
            }
        }
    }
    if (success && duplicateOperands.size() != violatingOperands.size()) {
        myCerr << "violatingOperands has too many indices.\n";
        success = false;
    }
    if (!success) {
        myCerr << "hashIndicesMap: " << hashIndicesMap << endl;
        myCerr << "indicesHashMap: " << indicesHashMap << endl;
        vector<UInt> sortedViolatingOperands(violatingOperands.begin(),
                                             violatingOperands.end());
        sort(sortedViolatingOperands.begin(), sortedViolatingOperands.end());

        myCerr << "violatingOperands(sorted): " << sortedViolatingOperands
               << endl;
        assert(false);
        myAbort();
    }
}

template struct SimpleUnaryOperator<BoolView, SequenceView, OpAllDiff>;

string OpAllDiff::getOpName() const { return "OpAllDiff"; }

void OpAllDiff::debugSanityCheckImpl() const {
    operand->debugSanityCheck();
    auto operandView = operand->getViewIfDefined();
    if (!operandView || operandView->numberUndefined > 0) {
        sanityLargeViolationCheck(violation);
        return;
    }
    sanityCheck(
        operandView->numberElements() == indicesHashMap.size(),
        toString("operand has ", operandView->numberElements(),
                 " elements but indicesHashMap has ", indicesHashMap.size()));
    lib::visit(
        [&](auto& members) {
            for (size_t i = 0; i < members.size(); i++) {
                auto view = members[i]->getViewIfDefined();
                if (!view) {
                    continue;
                }
                HashType hash = getValueHash(*view);
                sanityCheck(
                    hash == indicesHashMap[i],
                    toString("index ", i, " has hash ", hash,
                             " but indicesHashMap has ", indicesHashMap[i]));
            }
        },
        operandView->members);
    for (size_t i = 0; i < indicesHashMap.size(); i++) {
        HashType hash = indicesHashMap[i];
        auto iter = hashIndicesMap.find(hash);
        sanityCheck(iter != hashIndicesMap.end(),
                    toString("hash ", hash, " at index ", i,
                             " mapped to no set in hashIndicesMap."));
        auto& indices = iter->second;
        sanityCheck(indices.count(i),
                    toString("index ", i, " maps to hash ", " hash ",
                             " but hashIndicesMap does not have the hash index "
                             "mapping\n",
                             "Instead: Hash ", hash, " maps to ", indices));
    }
    size_t total = 0;
    vector<Int> duplicateOperands;
    for (auto& hashIndicesPair : hashIndicesMap) {
        total += hashIndicesPair.second.size();
        if (hashIndicesPair.second.size() > 1) {
            for (const auto& index : hashIndicesPair.second) {
                duplicateOperands.push_back(index);
            }
        }
    }
    sanityCheck(total == indicesHashMap.size(),
                toString("found ", total,
                         " indices in hashIndicesMap but indicesHashMap has a "
                         "size "
                         "of ",
                         indicesHashMap.size()));

    for (auto& index : duplicateOperands) {
        sanityCheck(
            violatingOperands.count(index),
            toString("violatingOperands does not contain index ", index));
    }
    sanityCheck(duplicateOperands.size() == violatingOperands.size(),
                toString("violatingOperands has too many indices."));
    UInt calcViolation = 0;
    for (auto& hashIndexPair : hashIndicesMap) {
        calcViolation += (hashIndexPair.second.size() - 1);
    }
    sanityEqualsCheck(calcViolation, violation);
}

template <typename Op>
struct OpMaker;

template <>
struct OpMaker<OpAllDiff> {
    static ExprRef<BoolView> make(ExprRef<SequenceView>);
};

ExprRef<BoolView> OpMaker<OpAllDiff>::make(ExprRef<SequenceView> o) {
    return make_shared<OpAllDiff>(move(o));
}
