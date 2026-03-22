#pragma once

#include <vector>
#include <string>

namespace mdmtsp_minmax {

struct Instance {
    std::vector<std::vector<double>> dist;
    std::vector<int> depots;

    // Effective vehicle assignment per depot index in `depots`.
    std::vector<int> depot_vehicle_limits;
    int k_vehicles = 0; // total vehicles (sum(depot_vehicle_limits))

    [[nodiscard]] int N() const { return static_cast<int>(dist.size()); }
    [[nodiscard]] std::vector<int> Customers() const;
    [[nodiscard]] double RouteLength(int depot, const std::vector<int>& route) const;
    [[nodiscard]] std::vector<int> ExpandedDepotVehicles() const;
};

Instance ParseInstanceFromJson(const std::string& payload);

} // namespace mdmtsp_minmax
