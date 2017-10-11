#include "utils/random.h"

std::random_device rand_dev;
std::mt19937 globalRandomGenerator(rand_dev());
