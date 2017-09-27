#include <iostream>
#include <string>
#include <vector>
#include "common/common.h"
#include "picosha2.h"
void hashExperiments() {
    std::string str = "The quick brown fox jumps over the lazy dog";
    std::vector<int> vec(str.begin(), str.end());
    ;
    std::vector<int> vec2(str.begin(), str.end());
    ;
    std::random_shuffle(vec2.begin(), vec2.end());

    std::vector<unsigned char> hash(32);
    picosha2::hash256(vec, hash);
    std::vector<unsigned char> hash2(1);
    picosha2::hash256(vec2, hash2);
    std::vector<unsigned int> hash3(4);
    picosha2::hash256(vec, hash3);
    std::cout << std::hex << hash3 << std::endl;
    std::vector<unsigned int> hash4(hash.begin(), hash.end());
    std::cout << std::hex << hash4 << std::endl;
}
