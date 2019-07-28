#ifndef SRC_OPERATORS_EMPTYORVIOLATINGOPTIONAL_H_
#define SRC_OPERATORS_EMPTYORVIOLATINGOPTIONAL_H_
#include "base/exprRef.h"

/*helper functions, returns empty optional if not BoolView return type otherwise
 * returns violating BoolView. */

template <typename View>
inline OptionalRef<View> emptyOrViolatingOptional() {
    return EmptyOptional();
}

template <>
inline OptionalRef<BoolView> emptyOrViolatingOptional<BoolView>() {
    return VIOLATING_BOOL_VIEW;
}

#endif /* SRC_OPERATORS_EMPTYORVIOLATINGOPTIONAL_H_ */
