/*
 * ignoreUnused.h
 *
 *  Created on: 27 Sep 2017
 *      Author: SaadAttieh
 */

#ifndef SRC_UTILS_IGNOREUNUSED_H_
#define SRC_UTILS_IGNOREUNUSED_H_
template <typename... Types>
void ignoreUnused(Types&&...) {}

#define todoImpl(...)                                             \
    ignoreUnused(__VA_ARGS__);                                    \
    std::cerr << "Error, this function yet to be implemented.\n"; \
    assert(false);                                                \
    abort();
#endif /* SRC_UTILS_IGNOREUNUSED_H_ */
