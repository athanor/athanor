#ifndef COMMON_COMMON_H
#define COMMON_COMMON_H
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include "hash.h"
#include "toString.h"
#include "tupleForEach.h"

#ifdef DEBUG_MODE
//#define LOG_WITH_THREAD_NAME
#ifdef LOG_WITH_THREAD_NAME
#define debug_thread_log \
    std::cout << "thread " << std::this_thread::get_id() << ":"
#else
#define debug_thread_log std::cout
#endif

extern bool debugLogAllowed;
#define debug_log_no_endl(x)                                           \
    if (debugLogAllowed) {                                             \
        std::lock_guard<std::recursive_mutex> guard(COMMON_LOG_MUTEX); \
        debug_thread_log << x;                                         \
    }
#define debug_log(x)                                                   \
    if (debugLogAllowed) {                                             \
        std::lock_guard<std::recursive_mutex> guard(COMMON_LOG_MUTEX); \
        debug_thread_log << x << std::endl;                            \
    }
#define debug_code(x) x
#else
#define debug_log(x)
#define debug_log_no_endl(x)
#define debug_code(x)
#endif

template <typename T>
using BaseType =
    typename std::remove_cv<typename std::remove_reference<T>::type>::type;

extern std::recursive_mutex COMMON_LOG_MUTEX;
#endif  // common_common_h
