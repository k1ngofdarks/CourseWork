#pragma once

#include <vector>
#include <string>

namespace mdmtsp_minmax {

struct Instance {
    std::vector<std::vector<double>> dist;
    std::vector<int> depots;
    int k_vehicles = 0;

    [[nodiscard]] int N() const { return static_cast<int>(dist.size()); }
    [[nodiscard]] std::vector<int> Customers() const;
    [[nodiscard]] double RouteLength(int depot, const std::vector<int>& route) const;
};

Instance ParseInstanceFromJson(const std::string& payload);

} // namespace mdmtsp_minmax
