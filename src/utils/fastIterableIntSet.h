#ifndef SRC_UTILS_FASTITERABLEINTSET_H_
#define SRC_UTILS_FASTITERABLEINTSET_H_
#include <iostream>
#include <vector>
#include "base/intSize.h"
#include "common/common.h"
class FastIterableIntSet {
    static const int NOT_PRESENT = 0;
    Int minElement;
    std::vector<Int> contents;
    std::vector<size_t> elementIndexes;

   public:
    FastIterableIntSet(const Int minElement, const Int maxElement)
        : minElement(minElement),
          elementIndexes((maxElement + 1) - minElement, NOT_PRESENT) {}

    FastIterableIntSet() : FastIterableIntSet(0, 0) {}
    inline void increaseMaxElement(size_t maxElement) {
        if ((maxElement + 1) - minElement < elementIndexes.size()) {
            return;
        }
        elementIndexes.resize((maxElement + 1) - minElement, NOT_PRESENT);
    }

    inline int count(Int element) {
        size_t index = translate(element);
        return index < elementIndexes.size() &&
               elementIndexes[index] != NOT_PRESENT;
    }

    inline auto insert(Int element) {
        increaseMaxElement(element);
        if (count(element)) {
            return std::make_pair(contents.begin() + getIndexOf(element),
                                  false);
        } else {
            contents.push_back(element);
            setIndexOf(element, contents.size() - 1);

            return std::make_pair(contents.begin() + (contents.size() - 1),
                                  true);
        }
    }

    inline int erase(Int element) {
        if (!count(element)) {
            return 0;
        } else {
            size_t index = getIndexOf(element);
            contents[index] = contents.back();
            setIndexOf(contents[index], index);
            contents.pop_back();
            elementIndexes[translate(element)] = NOT_PRESENT;
            return 1;
        }
    }

    inline auto begin() const { return contents.cbegin(); }
    inline auto cbegin() const { return contents.cbegin(); }
    inline auto end() const { return contents.cend(); }
    inline auto cend() const { return contents.cend(); }
    inline size_t size() const { return contents.size(); }
    inline bool empty() const { return size() == 0; }

    inline void clear() {
        contents.clear();
        elementIndexes.clear();
    }

   private:
    inline size_t translate(Int element) { return element - minElement; }

    inline size_t getIndexOf(Int element) {
        return elementIndexes[translate(element)] - 1;
    }
    inline void setIndexOf(Int element, size_t index) {
        elementIndexes[translate(element)] = index + 1;
    }
};

#endif /* SRC_UTILS_FASTITERABLEINTSET_H_ */
