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

u_int64_t DefinesLock::globalStamp = 1;
deque<AnyDefinedVarTrigger> definedVarTriggerQueue;
deque<AnyDefinedVarTrigger> delayedDefinedVarTriggerQueue;

typename deque<AnyDefinedVarTrigger>::iterator findNextTrigger(
    deque<AnyDefinedVarTrigger>& queue);
auto& active(AnyDefinedVarTrigger& t) {
    return mpark::visit([](auto& t) -> bool& { return t.active(); }, t);
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

// the following functions are helpers to work out which derived class of
// ValBase we  have and to apply a visitor to it

template <typename Visitor, typename NoMatchHandler>
inline void visitDerivedTypeHelper(Visitor&&, NoMatchHandler&& noMatchHandler,
                                   ValBase*) {
    noMatchHandler();
}

template <typename Visitor, typename NoMatchHandler, typename T, typename... Ts>
inline void visitDerivedTypeHelper(Visitor&& visitor,
                                   NoMatchHandler&& noMatchHandler,
                                   ValBase* base) {
    T* derived = dynamic_cast<T*>(base);
    if (derived) {
        visitor(*derived);
    } else {
        visitDerivedTypeHelper<Visitor, NoMatchHandler, Ts...>(
            forward<Visitor>(visitor), forward<NoMatchHandler>(noMatchHandler),
            base);
    }
}

template <typename Visitor, typename NoMatchHandler>
void visitDerivedType(Visitor&& visitor, NoMatchHandler&& noMatchHandler,
                      ValBase* base) {
    visitDerivedTypeHelper<Visitor, NoMatchHandler, SequenceValue,
                           FunctionValue, TupleValue>(
        forward<Visitor>(visitor), forward<NoMatchHandler>(noMatchHandler),
        base);
}

bool supportsDefinedElements(const FunctionValue&) { return true; }
bool supportsDefinedElements(const SequenceValue& val) {
    return !val.injective;
}
bool supportsDefinedElements(const TupleValue&) { return true; }
// functions for updating the containers when their element has been changed
// because of the element being defined on  an expression which changed.
bool isInContainerSupportingDefinedVars(ValBase* container) {
    if (container == &variablePool) {
        return true;
    } else if (!container || isPoolMarker(container)) {
        return false;
    }
    bool isSupported = true;
    visitDerivedType(
        [&](auto& val) { isSupported &= supportsDefinedElements(val); },
        [&]() { isSupported = false; }, container);
    return isSupported &&
           isInContainerSupportingDefinedVars(container->container);
}

void notifyType(const ValBase* base, FunctionValue& val);
void notifyType(const ValBase* base, SequenceValue& val);
void notifyType(const ValBase* base, TupleValue& val);

void updateParentContainer(ValBase* container) {
    if (!container || isPoolMarker(container)) {
        return;
    }
    visitDerivedType([&](auto& val) { notifyType(container, val); },
                     [&]() { shouldNotBeCalledPanic; }, container);
    updateParentContainer(container->container);
}

void notifyType(const ValBase* base, SequenceValue& val) {
    mpark::visit(
        [&](auto& members) {
            val.changeSubsequenceAndNotify<viewType(members)>(base->id,
                                                              base->id + 1);
        },
        val.members);
}

void notifyType(const ValBase* base, FunctionValue& val) {
    val.notifyImageChanged(base->id);
}

void notifyType(const ValBase* base, TupleValue& val) {
    debug_code(assert(base->id < val.members.size()));
    val.memberChangedAndNotify(base->id);
}
