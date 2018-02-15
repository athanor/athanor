#include <iostream>
#include <json.hpp>
using namespace std;
using namespace nlohmann;
void jsonTest() {
    json j;
    cin >> j;
    cout << j["mStatements"] << endl;
}
