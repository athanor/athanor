#include "operators/definedVarHelper.h"

#include <algorithm>

#include "operators/definedVarHelper.hpp"
#include "operators/opBoolEq.h"
#include "operators/opEnumEq.h"
#include "operators/opIntEq.h"
#include "types/boolVal.h"
#include "types/enumVal.h"
#include "types/functionVal.h"
#include "types/intVal.h"
#include "types/sequenceVal.h"
#include "types/tupleVal.h"
using namespace std;

UInt64 DefinesLock::globalStamp = 1;
deque<AnyDefinedVarTrigger> definedVarTriggerQueue;
deque<AnyDefinedVarTrigger> delayedDefinedVarTriggerQueue;

typename deque<AnyDefinedVarTrigger>::iterator findNextTrigger(
    deque<AnyDefinedVarTrigger>& queue);
auto& active(AnyDefinedVarTrigger& t) {
    return lib::visit([](auto& t) -> bool& { return t.active(); }, t);
}

void handleDefinedVarTriggers() {
    auto triggerHandler = [&](auto& trigger) {
        if (trigger.active()) {
            trigger.trigger();
        }
    };
    while (!delayedDefinedVarTriggerQueue.empty() ||
           !definedVarTriggerQueue.empty()) {
        while (!definedVarTriggerQueue.empty()) {
            lib::visit(triggerHandler, definedVarTriggerQueue.front());
            definedVarTriggerQueue.pop_front();
        }
        DefinesLock::unlockAll();
        while (!delayedDefinedVarTriggerQueue.empty() &&
               !active(delayedDefinedVarTriggerQueue.front())) {
            delayedDefinedVarTriggerQueue.pop_front();
        }
        if (!delayedDefinedVarTriggerQueue.empty()) {
            auto iter = findNextTrigger(delayedDefinedVarTriggerQueue);
            lib::visit(triggerHandler, *iter);
            active(*iter) = false;
        }
    }
}

typename deque<AnyDefinedVarTrigger>::iterator findNextTrigger(
    deque<AnyDefinedVarTrigger>& queue) {
    auto iter = find_if(queue.begin(), queue.end(), [](auto& t) {
        return lib::visit(
            [&](auto& t) {
                return t.active() &&
                       t.definedDirection != DefinedDirection::BOTH;
            },
            t);
    });
    return (iter != queue.end()) ? iter : queue.begin();
}

// the following functions are helpers to work out which derived class of
// ValBase we  have and to apply a visitor to it

template <typename Visitor, typename NoMatchHandler>
inline void visitDerivedTypeHelper(Visitor&&, NoMatchHandler&& noMatchHandler,
                                   ValBase*) {
    noMatchHandler();
}

// functions for updating the containers when their element has been changed
// because of the element being defined on  an expression which changed.
bool isInContainerSupportingDefinedVars(ValBase* container) {
    if (container == &variablePool) {
        return true;
    } else if (!container || isPoolMarker(container)) {
        return false;
    }
    return container->supportsDefinedVars() &&
           isInContainerSupportingDefinedVars(container->container);
}

void updateParentContainer(ValBase* container, UInt childId) {
    if (!container || isPoolMarker(container)) {
        return;
    }
    container->notifyVarDefined(childId);
    updateParentContainer(container->container, container->id);
}
