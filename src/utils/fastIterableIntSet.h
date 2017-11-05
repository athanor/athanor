
#ifndef SRC_UTILS_FASTITERABLEINTSET_H_
#define SRC_UTILS_FASTITERABLEINTSET_H_
#include <iostream>
#include <vector>
#include "common/common.h"
class FastIterableIntSet {
    static const int NOT_PRESENT = 0;
    const int64_t minElement;
    std::vector<int64_t> contents;
    std::vector<size_t> elementIndexes;

   public:
    FastIterableIntSet(const int64_t minElement, const int64_t maxElement)
        : minElement(minElement),
          elementIndexes((maxElement + 1) - minElement, NOT_PRESENT) {}

    inline int count(int64_t element) {
        return elementIndexes[translate(element)] != NOT_PRESENT;
    }

    inline auto insert(int64_t element) {
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

    inline int erase(int64_t element) {
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

   private:
    inline size_t translate(int64_t element) { return element - minElement; }

    inline size_t getIndexOf(int64_t element) {
        return elementIndexes[translate(element)] - 1;
    }
    inline void setIndexOf(int64_t element, size_t index) {
        elementIndexes[translate(element)] = index + 1;
    }
};

#endif /* SRC_UTILS_FASTITERABLEINTSET_H_ */
