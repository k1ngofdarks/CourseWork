#pragma once

#include <vector>

namespace mdmtsp_minmax {

struct Instance {
    std::vector<std::vector<double>> dist;
    std::vector<int> depots;
    int k_vehicles = 0;
};

} // namespace mdmtsp_minmax
