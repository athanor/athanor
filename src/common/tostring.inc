#include <iostream>
#include <list>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

/** varadic to string function
 *
 */
template <typename... Args>
std::string toString(Args const&... args) {
    std::ostringstream result;
    int unpack[]{0, (result << args, 0)...};
    static_cast<void>(unpack);
    return result.str();
}

template <typename T>
std::ostream& containerToArrayString(std::ostream& os, const T container,
                                     const std::string& separator = ",");
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T> listIn);
template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& os, const std::pair<T1, T2> pairIn);
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T> vec);
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T> listIn);
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T> setIn);
template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T> setIn);
template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& os, const std::map<T1, T2> mapIn);
template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& os, const std::pair<T1, T2> pairIn) {
    os << "(" << pairIn.first << "," << pairIn.second << ")";
    return os;
}

template <typename T>
std::ostream& containerToArrayString(std::ostream& os, const T container,
                                     const std::string& separator) {
    bool first = true;
    os << '[';
    for (auto value : container) {
        if (first) {
            first = false;
        } else {
            os << separator;
        }
        os << value;
    }
    os << ']';
    return os;
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T> vec) {
    return containerToArrayString(os, vec);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::list<T> listIn) {
    return containerToArrayString(os, listIn);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::set<T> setIn) {
    return containerToArrayString(os, setIn);
}

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::unordered_set<T> setIn) {
    return containerToArrayString(os, setIn);
}

template <typename T1, typename T2>
std::ostream& operator<<(std::ostream& os, const std::map<T1, T2> mapIn) {
    return containerToArrayString(os, mapIn);
}
