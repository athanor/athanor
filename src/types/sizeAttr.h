
#ifndef SRC_TYPES_SIZEATTR_H_
#define SRC_TYPES_SIZEATTR_H_
#include "base/intSize.h"
struct SizeAttr {
    enum class SizeAttrType {
        NO_SIZE,
        EXACT_SIZE,
        MIN_SIZE,
        MAX_SIZE,
        SIZE_RANGE
    };
    SizeAttrType sizeType;
    size_t minSize, maxSize;

   private:
    SizeAttr(SizeAttrType type, size_t minSize, size_t maxSize)
        : sizeType(type), minSize(minSize), maxSize(maxSize) {}

   public:
    friend inline SizeAttr noSize() {
        return SizeAttr(SizeAttrType::NO_SIZE, 0,
                        std::numeric_limits<UInt>::max());
    }
    friend inline SizeAttr exactSize(size_t size) {
        return SizeAttr(SizeAttrType::EXACT_SIZE, size, size);
    }
    friend inline SizeAttr minSize(size_t minSize) {
        return SizeAttr(SizeAttrType::MIN_SIZE, minSize,
                        std::numeric_limits<UInt>::max());
    }
    friend inline SizeAttr maxSize(size_t maxSize) {
        return SizeAttr(SizeAttrType::MAX_SIZE, 0, maxSize);
    }

    friend inline SizeAttr sizeRange(size_t minSize, size_t maxSize) {
        return SizeAttr(SizeAttrType::SIZE_RANGE, minSize, maxSize);
    }
    friend inline std::ostream& operator<<(std::ostream& os,
                                           const SizeAttr& s) {
        switch (s.sizeType) {
            case SizeAttrType::NO_SIZE:
                os << "noSize";
                break;
            case SizeAttrType::MIN_SIZE:
                os << "minSize " << s.minSize;
                break;
            case SizeAttrType::MAX_SIZE:
                os << "maxSize " << s.maxSize;
                break;
            case SizeAttrType::EXACT_SIZE:
                os << "size " << s.minSize;
                break;
            case SizeAttrType::SIZE_RANGE:
                os << "minSize " << s.minSize << ", maxSize " << s.maxSize;
        }
        return os;
    }
};

SizeAttr noSize();
inline SizeAttr exactSize(size_t size);
SizeAttr minSize(size_t minSize);
SizeAttr maxSize(size_t maxSize);
SizeAttr sizeRange(size_t minSize, size_t maxSize);

#endif /* SRC_TYPES_SIZEATTR_H_ */
