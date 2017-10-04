
#ifndef SRC_TYPES_BOOL_H_
#define SRC_TYPES_BOOL_H_
#include <utility>
#include <vector>

#include "forwardDecls/typesAndDomains.h"
#include "operators/boolProducing.h"
struct BoolDomain {};
struct BoolValue : DirtyFlag {
    bool value;
    std::vector<std::shared_ptr<BoolTrigger>> triggers;

    template <typename Func>
    inline void changeValue(Func&& func) {
        for (std::shared_ptr<BoolTrigger>& t : triggers) {
            t->preValueChange(*this);
        }
        func();
        for (std::shared_ptr<BoolTrigger>& t : triggers) {
            t->postValueChange(*this);
        }
    }
};

#endif /* SRC_TYPES_BOOL_H_ */
