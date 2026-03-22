#pragma once

#include <vector>

namespace tsp {

struct TSPSolution {
    std::vector<int> route;
    double length = 0.0;
};

} // namespace tsp
