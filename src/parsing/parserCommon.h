#ifndef SRC_PARSING_PARSERCOMMON_H_
#define SRC_PARSING_PARSERCOMMON_H_
#include <iostream>
#include <json.hpp>
#include <memory>
#include <unordered_map>
#include "base/base.h"
// refactorring the json model parser into several files.  Just started, this
// will be a long process.  This is the beginnings of a shared common header
// which will contain the necessary prototypes of course.

bool hasEmptyDomain(const AnyDomainRef& domain);
void removeEmptyType(const AnyDomainRef& domain, AnyValRef& val);
#endif /* SRC_PARSING_PARSERCOMMON_H_ */
