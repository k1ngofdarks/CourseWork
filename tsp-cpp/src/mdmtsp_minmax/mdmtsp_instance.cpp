#include "mdmtsp_minmax/mdmtsp_instance.h"
#include <json.hpp>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace mdmtsp_minmax {

namespace {

std::vector<std::vector<double>> BuildEuclidean(const std::vector<double>& x, const std::vector<double>& y) {
    size_t n = x.size();
    std::vector<std::vector<double>> d(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double dx = x[i] - x[j];
            double dy = y[i] - y[j];
            d[i][j] = std::sqrt(dx * dx + dy * dy);
        }
    }
    return d;
}

std::vector<std::vector<double>> BuildHaversine(const std::vector<double>& lat_deg, const std::vector<double>& lon_deg) {
    const double R = 6371.0;
    size_t n = lat_deg.size();
    std::vector<double> lat(n), lon(n);
    for (size_t i = 0; i < n; ++i) {
        lat[i] = lat_deg[i] * M_PI / 180.0;
        lon[i] = lon_deg[i] * M_PI / 180.0;
    }
    std::vector<std::vector<double>> d(n, std::vector<double>(n, 0.0));
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            double dlat = std::sin((lat[j] - lat[i]) / 2.0);
            double dlon = std::sin((lon[j] - lon[i]) / 2.0);
            double h = dlat * dlat + std::cos(lat[i]) * std::cos(lat[j]) * dlon * dlon;
            d[i][j] = R * 2.0 * std::atan2(std::sqrt(h), std::sqrt(1.0 - h));
        }
    }
    return d;
}

std::vector<int> ParseVehicleLimits(const nlohmann::json& j, const std::vector<int>& depots) {
    if (j.contains("depot_vehicle_limits")) {
        const auto& lim = j.at("depot_vehicle_limits");
        if (lim.is_array()) {
            auto arr = lim.get<std::vector<int>>();
            if (arr.size() != depots.size()) {
                throw std::invalid_argument("depot_vehicle_limits array must match depots size");
            }
            return arr;
        }

        if (lim.is_object()) {
            std::vector<int> out(depots.size(), 0);
            for (size_t i = 0; i < depots.size(); ++i) {
                std::string key = std::to_string(depots[i]);
                out[i] = lim.contains(key) ? lim.at(key).get<int>() : 0;
            }
            return out;
        }
    }

    if (j.contains("k_vehicles")) {
        int total = j.at("k_vehicles").get<int>();
        if (total <= 0) throw std::invalid_argument("k_vehicles must be positive");
        std::vector<int> out(depots.size(), 0);
        for (int i = 0; i < total; ++i) out[i % static_cast<int>(depots.size())]++;
        return out;
    }

    throw std::invalid_argument("mdmtsp_minmax requires k_vehicles or depot_vehicle_limits");
}

} // namespace

std::vector<int> Instance::Customers() const {
    std::vector<char> is_depot(N(), 0);
    for (int d : depots) {
        if (d >= 0 && d < N()) is_depot[d] = 1;
    }
    std::vector<int> customers;
    for (int i = 0; i < N(); ++i) {
        if (!is_depot[i]) customers.push_back(i);
    }
    return customers;
}

double Instance::RouteLength(int depot, const std::vector<int>& route) const {
    if (route.empty()) return 0.0;
    double sum = dist[depot][route.front()];
    for (size_t i = 0; i + 1 < route.size(); ++i) {
        sum += dist[route[i]][route[i + 1]];
    }
    sum += dist[route.back()][depot];
    return sum;
}

std::vector<int> Instance::ExpandedDepotVehicles() const {
    std::vector<int> route_depots;
    for (size_t i = 0; i < depots.size(); ++i) {
        for (int c = 0; c < depot_vehicle_limits[i]; ++c) route_depots.push_back(depots[i]);
    }
    return route_depots;
}

Instance ParseInstanceFromJson(const std::string& payload) {
    auto j = nlohmann::json::parse(payload);

    Instance inst;
    inst.depots = j.at("depots").get<std::vector<int>>();
    if (inst.depots.empty()) throw std::invalid_argument("depots array must be non-empty");

    inst.depot_vehicle_limits = ParseVehicleLimits(j, inst.depots);
    inst.k_vehicles = std::accumulate(inst.depot_vehicle_limits.begin(), inst.depot_vehicle_limits.end(), 0);

    std::string format = j.value("format", "matrix");
    if (format == "matrix") {
        inst.dist = j.at("matrix").get<std::vector<std::vector<double>>>();
        return inst;
    }

    auto coords = j.at("coords");
    std::vector<double> x = coords.at(0).get<std::vector<double>>();
    std::vector<double> y = coords.at(1).get<std::vector<double>>();
    std::string metric = j.value("metric", "euclidean");
    inst.dist = (metric == "haversine") ? BuildHaversine(x, y) : BuildEuclidean(x, y);
    return inst;
}

} // namespace mdmtsp_minmax
