#include <iostream>
#include <string>
#include <vector>
#include "common/common.h"
#include "picosha2.h"
void hashExperiments() {
    std::string src_str = "The quick brown fox jumps over the lazy dog";

    std::vector<unsigned char> hash(32);
    picosha2::hash256(src_str, hash);
    std::cout << hash << std::endl;
}
