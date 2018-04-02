
#ifndef SRC_OPERATORS_SHIFTVIOLATINGINDICES_H_
#define SRC_OPERATORS_SHIFTVIOLATINGINDICES_H_
#include <vector>
#include "utils/fastIterableIntSet.h"
inline void shiftIndicesUp(UInt fromIndex, UInt newNumberElements,
                           FastIterableIntSet& indexSet) {
    static thread_local std::vector<UInt> tempIndicesBuffer;
    if (newNumberElements - fromIndex <= indexSet.size()) {
        // it's faster to shift indices by scanning entire shifted range
        for (size_t i = newNumberElements - 1; i > fromIndex; i--) {
            if (indexSet.erase(i - 1)) {
                indexSet.insert(i);
            }
        }
    } else {
        // it's faster to iterate through set, finding indices that require
        // shifting
        for (UInt violatingIndex : indexSet) {
            if (violatingIndex >= fromIndex) {
                tempIndicesBuffer.push_back(violatingIndex);
            }
        }
        for (UInt violatingIndex : tempIndicesBuffer) {
            indexSet.erase(violatingIndex);
        }
        for (UInt violatingIndex : tempIndicesBuffer) {
            indexSet.insert(violatingIndex + 1);
        }
        tempIndicesBuffer.clear();
    }
}

inline void shiftIndicesDown(UInt fromIndex, UInt newNumberElements,
                             FastIterableIntSet& indexSet) {
    static thread_local std::vector<UInt> tempIndicesBuffer;
    // need to shift indices marked as violating
    if (newNumberElements - fromIndex <= indexSet.size()) {
        // it's faster to shift indices by scanning entire shifted range
        for (size_t i = fromIndex; i < newNumberElements; i++) {
            if (indexSet.erase(i + 1)) {
                indexSet.insert(i);
            }
        }
    } else {
        // it's faster to iterate through set, finding indices that require
        // shifting
        for (UInt violatingIndex : indexSet) {
            if (violatingIndex > fromIndex) {
                tempIndicesBuffer.push_back(violatingIndex);
            }
        }
        for (UInt violatingIndex : tempIndicesBuffer) {
            indexSet.erase(violatingIndex);
        }
        for (UInt violatingIndex : tempIndicesBuffer) {
            indexSet.insert(violatingIndex - 1);
        }
        tempIndicesBuffer.clear();
    }
}
#endif /* SRC_OPERATORS_SHIFTVIOLATINGINDICES_H_ */
