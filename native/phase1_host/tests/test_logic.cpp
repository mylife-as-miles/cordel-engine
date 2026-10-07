// Copyright (c) 2026 CORDEL contributors. MIT.
#include "logic_suite.hpp"
#include <iostream>
int main() {
    try {
        auto count=cordel::logic_checks(CORDEL_DEFAULT_SCENE);
        std::cout<<"Native math/input/fixed timing/ownership/shared golden checks: "<<count<<" passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
