#ifndef SRC_TYPES_VALUE_H_
#define SRC_TYPES_VALUE_H_
#include <memory>
#include "variantOperations.h"

struct SetValue;
struct IntValue;
using Value = mpark::variant<std::shared_ptr<SetValue>, IntValue>;
#endif /* SRC_TYPES_VALUE_H_ */
