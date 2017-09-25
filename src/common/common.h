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

#ifdef DEBUG_MODE
//#define LOG_WITH_THREAD_NAME
#ifdef LOG_WITH_THREAD_NAME
#define debug_thread_log \
    std::cout << "thread " << std::this_thread::get_id() << ":"
#else
#define debug_thread_log std::cout
#endif

#define debug_log_no_endl(x)                                           \
    {                                                                  \
        std::lock_guard<std::recursive_mutex> guard(COMMON_LOG_MUTEX); \
        debug_thread_log << x;                                         \
    }
#define debug_log(x)                                                   \
    {                                                                  \
        std::lock_guard<std::recursive_mutex> guard(COMMON_LOG_MUTEX); \
        debug_thread_log << x << std::endl;                            \
    }
#define debug_code(x) x
#ifdef LOG_TRACE
#define debug_log_trace(x) debug_log(x)
#else
#define debug_log_trace(x)  //
#endif
#ifdef LOG_PARTITION
#define debug_log_partition(x) debug_log(x)
#else
#define debug_log_partition(x)  //
#endif
#else
#define debug_log(x)
#define debug_log_no_endl(x)
#define debug_code(x)
#define debug_log_trace(x)  //
#define debug_log_partition(x)  //
#endif



template <std::size_t I = 0, typename FuncT, typename... Tp>
inline typename std::enable_if<I == sizeof...(Tp), bool>::type forEach(
    const std::tuple<Tp...> &, const FuncT &) {
    return true;
}

template <std::size_t I = 0, typename FuncT, typename... Tp>
    inline typename std::enable_if <
    I<sizeof...(Tp), bool>::type forEach(const std::tuple<Tp...> &t,
                                         const FuncT &f) {
    if (!f(std::get<I>(t))) {
        return false;
    }
    return forEach<I + 1, FuncT, Tp...>(t, f);
}

#include "hash.inc"
#include "tostring.inc"

template <typename T>
T fromString(const std::string &str) {
    std::istringstream iss(str);
    T obj;
    iss >> std::ws >> obj >> std::ws;
    if (!iss.eof())
        throw std::runtime_error(toString("Failed to parse  ``", str, "''"));
    return obj;
}

extern std::recursive_mutex COMMON_LOG_MUTEX;
#endif  // common_common_h
