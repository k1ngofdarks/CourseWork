#pragma once

#include <vector>

namespace mdmtsp_minmax {

struct Route {
    int depot_id = -1;
    std::vector<int> nodes;
    double length = 0.0;
};

struct Solution {
    std::vector<Route> routes;
    double max_route_length = 0.0;
};

} // namespace mdmtsp_minmax
