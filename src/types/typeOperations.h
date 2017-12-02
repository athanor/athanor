
#ifndef SRC_TYPES_TYPEOPERATIONS_H_
#define SRC_TYPES_TYPEOPERATIONS_H_
#include <iostream>
#include "types/base.h"

// template forward declarations.  Template specialisations are assumed to be
// implemented in CPP files.
// That is, every type must provide a specialisation of the following functions.

template <typename Val>
bool smallerValue(const Val& u, const Val& v);
template <typename Val>
bool largerValue(const Val& u, const Val& v);
template <typename Val>
bool equalValue(const Val& u, const Val& v);
template <typename Val>
u_int64_t getValueHash(const Val&);
template <typename Val>
void normalise(Val& v);
template <typename Val>
void deepCopy(const Val& src, Val& target);
template <typename DomainType>
u_int64_t getDomainSize(const DomainType&);
template <typename Val>
std::ostream& prettyPrint(std::ostream& os, const Val&);
template <typename DomainType>
std::ostream& prettyPrint(std::ostream& os, const DomainType&);

// variant dispatches to invoke the above functions on variant types.  They have
// a unused to template to  delay compilation till invoked.

template <typename T = int>
inline bool smallerValue(const AnyValRef& u, const AnyValRef& v, T = 0) {
    return u.index() < v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return smallerValue(*uImpl, *vImpl);
                },
                u));
}

template <typename T = int>
inline void normalise(AnyValRef& v, T = 0) {
    mpark::visit([](auto& vImpl) { normalise(*vImpl); }, v);
}

template <typename T = int>
inline bool largerValue(const AnyValRef& u, const AnyValRef& v, T = 0) {
    return u.index() > v.index() ||
           (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return largerValue(*uImpl, *vImpl);
                },
                u));
}

template <typename T = int>
inline bool equalValue(const AnyValRef& u, const AnyValRef& v, T = 0) {
    return (u.index() == v.index() &&
            mpark::visit(
                [&v](auto& uImpl) {
                    auto& vImpl = mpark::get<BaseType<decltype(uImpl)>>(v);
                    return equalValue(*uImpl, *vImpl);
                },
                u));
}

template <typename T = int>
inline u_int64_t getValueHash(const AnyValRef& val, T = 0) {
    return mpark::visit(
        [&](const auto& valImpl) { return getValueHash(*valImpl); }, val);
}

template <typename Val>
ValRef<Val> deepCopy(const Val& src) {
    auto target = constructValueOfSameType(src);
    deepCopy(src, *target);
    return target;
}

template <typename T = int>
inline AnyValRef deepCopy(const AnyValRef& val, T = 0) {
    return mpark::visit(
        [](const auto& valImpl) { return AnyValRef(deepCopy(*valImpl)); }, val);
}

template <typename T = int>
inline u_int64_t getDomainSize(const AnyDomainRef& domain, T = 0) {
    return mpark::visit(
        [&](const auto& domainImpl) { return getDomainSize(*domainImpl); },
        domain);
}

template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyDomainRef& d,
                                 T = 0) {
    mpark::visit([&os](auto& dImpl) { prettyPrint(os, *dImpl); }, d);
    return os;
}

template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyValRef& v, T = 0) {
    mpark::visit([&os](auto& vImpl) { prettyPrint(os, *vImpl); }, v);
    return os;
}

template <typename Val>
std::ostream& operator<<(std::ostream& os, const ValRef<Val>& v) {
    return prettyPrint(os, v);
}

inline std::ostream& operator<<(std::ostream& os, const AnyValRef& v) {
    return prettyPrint(os, v);
}
template <typename Val>
inline typename std::enable_if<IsValueType<Val>::value, std::ostream&>::type
operator<<(std::ostream& os, const Val& v) {
    return prettyPrint(os, v);
}

#endif /* SRC_TYPES_TYPEOPERATIONS_H_ */
