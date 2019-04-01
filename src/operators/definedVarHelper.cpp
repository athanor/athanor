#include "operators/definedVarHelper.h"
#include <algorithm>
#include "operators/opBoolEq.h"
#include "operators/opEnumEq.h"
#include "operators/opIntEq.h"
#include "types/boolVal.h"
#include "types/enumVal.h"
#include "types/intVal.h"
using namespace std;

u_int64_t DefinesLock::globalStamp = 1;
deque<AnyDefinedVarTrigger> definedVarTriggerQueue;
deque<AnyDefinedVarTrigger> delayedDefinedVarTriggerQueue;

typename deque<AnyDefinedVarTrigger>::iterator findNextTrigger(
    deque<AnyDefinedVarTrigger>& queue);
auto& active(AnyDefinedVarTrigger& t) {
    return mpark::visit([](auto& t) -> bool& { return t.active(); }, t);
}

void handleDefinedVarTriggers() {
    auto triggerHandler = overloaded(
        [&](DefinedVarTrigger<OpIntEq>& trigger) {
            if (trigger.active()) {
                trigger.trigger();
            }
        },
        [&](auto&) { todoImpl(); });
    while (!delayedDefinedVarTriggerQueue.empty() ||
           !definedVarTriggerQueue.empty()) {
        while (!definedVarTriggerQueue.empty()) {
            debug_log("Pulling from definedTriggerQueue");
            mpark::visit(triggerHandler, definedVarTriggerQueue.front());
            definedVarTriggerQueue.pop_front();
        }
        DefinesLock::unlockAll();
        while (!delayedDefinedVarTriggerQueue.empty() &&
               !active(delayedDefinedVarTriggerQueue.front())) {
            delayedDefinedVarTriggerQueue.pop_front();
        }
        if (!delayedDefinedVarTriggerQueue.empty()) {
            auto iter = findNextTrigger(delayedDefinedVarTriggerQueue);
            debug_log("Pulling from delayedDefinedTriggerQueue");
            mpark::visit(triggerHandler, *iter);
            active(*iter) = false;
                    }
    }
}

typename deque<AnyDefinedVarTrigger>::iterator findNextTrigger(
    deque<AnyDefinedVarTrigger>& queue) {
    auto iter = find_if(queue.begin(), queue.end(), [](auto& t) {
        return mpark::visit(
            [&](auto& t) {
                return t.active() &&
                       t.definedDirection != DefinedDirection::BOTH;
            },
            t);
    });
    return (iter != queue.end()) ? iter : queue.begin();
}
