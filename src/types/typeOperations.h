
#ifndef SRC_TYPES_TYPEOPERATIONS_H_
#define SRC_TYPES_TYPEOPERATIONS_H_
#include <iostream>
#include "types/base.h"

// template forward declarations.  Template specialisations are assumed to be
// implemented in CPP files.
// That is, every type must provide a specialisation of the following functions.

template <typename Val>
EnableIfValueAndReturn<Val, bool> smallerValue(const Val& u, const Val& v);
template <typename Val>
EnableIfValueAndReturn<Val, bool> largerValue(const Val& u, const Val& v);
template <typename Val>
EnableIfValueAndReturn<Val, bool> equalValue(const Val& u, const Val& v);
template <typename View>
EnableIfViewAndReturn<View, u_int64_t> getValueHash(const View&);
template <typename Val>
EnableIfValueAndReturn<Val, void> normalise(Val& v);
template <typename Val>
EnableIfValueAndReturn<Val, void> deepCopy(const Val& src, Val& target);
template <typename DomainType>
u_int64_t getDomainSize(const DomainType&);
template <typename View>
EnableIfViewAndReturn<View, std::ostream&> prettyPrint(std::ostream& os,
                                                       const View&);
template <typename DomainType>
typename std::enable_if<IsDomainType<BaseType<DomainType>>::value,
                        std::ostream&>::type
prettyPrint(std::ostream& os, const DomainType&);

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
inline u_int64_t getValueHash(const AnyViewRef& view, T = 0) {
    return mpark::visit(
        [&](const auto& viewImpl) { return getValueHash(*viewImpl); }, view);
}

template <typename Val>
EnableIfValueAndReturn<Val, ValRef<Val>> deepCopy(const Val& src) {
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
    return mpark::visit(
        [&os](auto& dImpl) -> std::ostream& { return prettyPrint(os, *dImpl); },
        d);
}

template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyValRef& v, T = 0) {
    return mpark::visit(
        [&os](auto& vImpl) -> std::ostream& {
            return prettyPrint(os, *getViewPtr(vImpl));
        },
        v);
}

template <typename T = int>
inline std::ostream& prettyPrint(std::ostream& os, const AnyViewRef& v, T = 0) {
    return mpark::visit(
        [&os](auto& vImpl) -> std::ostream& { return prettyPrint(os, *vImpl); },
        v);
}

inline std::ostream& operator<<(std::ostream& os, const AnyValRef& v) {
    return prettyPrint(os, v);
}

inline std::ostream& operator<<(std::ostream& os, const AnyDomainRef& d) {
    return prettyPrint(os, d);
}
template <typename View>
inline EnableIfViewAndReturn<View, std::ostream&> operator<<(std::ostream& os,
                                                             const View& v) {
    return prettyPrint(os, v);
}

template <typename View>
inline EnableIfViewAndReturn<View, std::ostream&> operator<<(
    std::ostream& os, const ViewRef<View>& v) {
    return prettyPrint(os, *v);
}

template <typename Domain>
inline typename std::enable_if<IsDomainType<BaseType<Domain>>::value,
                               std::ostream&>::type
operator<<(std::ostream& os, const Domain& d) {
    return prettyPrint(os, d);
}

#endif /* SRC_TYPES_TYPEOPERATIONS_H_ */
