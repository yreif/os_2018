//
// Created by yuval.reif on 6/17/18.
//
#include <iostream>
#include <vector>

class Test {
public:
    std::vector<std::string> names;
};

int main(int argc, char *argv[]) {
    Test t;
    std::cout << t.names.size() << std::endl;
    t.names.emplace_back("hey");
    std::cout << t.names.size() << std::endl;
}