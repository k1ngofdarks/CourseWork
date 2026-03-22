#include <instance.h>

#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include "../include/json.hpp"

namespace mdmtsp_minmax {
    inline std::string ReadAllStdin() {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        return ss.str();
    }

    inline std::vector<std::vector<double>> CalculateSphereDistances(const std::vector<double> &lat_deg,
                                                                      const std::vector<double> &lon_deg) {
        const double R = 6371.0;
        size_t n = lat_deg.size();
        std::vector<double> lat(n), lon(n);
        for (size_t i = 0; i < n; ++i) {
            lat[i] = lat_deg[i] * M_PI / 180;
            lon[i] = lon_deg[i] * M_PI / 180;
        }
        std::vector<std::vector<double>> dist(n, std::vector<double>(n));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double dlat_sin = sin((lat[j] - lat[i]) / 2);
                double dlon_sin = sin((lon[j] - lon[i]) / 2);
                double h = dlat_sin * dlat_sin + cos(lat[i]) * cos(lat[j]) * dlon_sin * dlon_sin;
                dist[i][j] = R * 2.0 * atan2(sqrt(h), sqrt(1.0 - h));
            }
        }
        return dist;
    }

    inline std::vector<std::vector<double>> CalculateEuclideanDistances(const std::vector<std::vector<double>> &coords) {
        const size_t n = coords.size();
        std::vector<std::vector<double>> dist(n, std::vector<double>(n));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double dx = coords[i][0] - coords[j][0];
                double dy = coords[i][1] - coords[j][1];
                dist[i][j] = std::sqrt(dx * dx + dy * dy);
            }
        }
        return dist;
    }

    inline std::vector<std::vector<double>> ParseMatrix(const nlohmann::json &json, int &n_out) {
        const nlohmann::json &matrix = json["matrix"];
        n_out = static_cast<int>(matrix.size());
        std::vector<std::vector<double>> dist(n_out, std::vector<double>(n_out));
        for (int i = 0; i < n_out; ++i) {
            if (!matrix[i].is_array() || static_cast<int>(matrix[i].size()) != n_out) {
                throw std::runtime_error("Matrix should be NxN array");
            }
            for (int j = 0; j < n_out; ++j) {
                dist[i][j] = matrix[i][j].get<double>();
            }
        }
        return dist;
    }

    inline std::vector<std::vector<double>> ParseCoordinates(const nlohmann::json &json, int &n_out) {
        const nlohmann::json &coords = json["coordinates"];
        n_out = static_cast<int>(coords.size());
        std::vector<std::vector<double>> points(n_out, std::vector<double>(2));
        for (int i = 0; i < n_out; ++i) {
            if (!coords[i].is_array() || coords[i].size() != 2) {
                throw std::runtime_error("Each coordinate should have size 2");
            }
            points[i][0] = coords[i][0].get<double>();
            points[i][1] = coords[i][1].get<double>();
        }

        std::string metric = json.value("metric", "euclidean");
        if (metric == "euclidean") return CalculateEuclideanDistances(points);
        if (metric == "sphere") {
            std::vector<double> lat(n_out), lon(n_out);
            for (int i = 0; i < n_out; ++i) {
                lat[i] = points[i][0];
                lon[i] = points[i][1];
            }
            return CalculateSphereDistances(lat, lon);
        }
        throw std::runtime_error("Unknown metric. Supported: euclidean, sphere");
    }

    inline std::vector<std::vector<double>> ParseLatLonDistances(const nlohmann::json &json, int n) {
        std::vector<double> lat(n), lon(n);
        const nlohmann::json &row_lat = json["latlon"][0];
        const nlohmann::json &row_lon = json["latlon"][1];
        for (int i = 0; i < n; ++i) {
            lat[i] = row_lat[i].get<double>();
            lon[i] = row_lon[i].get<double>();
        }
        return CalculateSphereDistances(lat, lon);
    }

    Instance::Instance() {
        auto json = nlohmann::json::parse(ReadAllStdin());
        if (!json.contains("depots") || !json["depots"].is_array() || json["depots"].empty()) {
            throw std::runtime_error("MDMTSP input should contain non-empty array depots");
        }

        for (const auto &depot_json: json["depots"]) {
            depots.push_back(depot_json.get<int>());
        }

        if (json.contains("matrix")) {
            mat = ParseMatrix(json, n);
        } else if (json.contains("coordinates")) {
            mat = ParseCoordinates(json, n);
        } else if (json.contains("latlon")) {
            n = json["n"].get<int>();
            mat = ParseLatLonDistances(json, n);
        } else {
            throw std::runtime_error("Invalid mdmtsp instance format. Expected one of: matrix, coordinates, latlon");
        }

        for (int depot: depots) {
            if (depot < 0 || depot >= n) {
                throw std::runtime_error("Depot index out of bounds");
            }
        }
    }

    int Instance::GetN() const { return n; }

    const std::vector<int> &Instance::GetDepots() const { return depots; }

    double Instance::Distance(int i, int j) const { return mat[i][j]; }

    double Instance::RouteLength(const std::vector<int> &route) const {
        double length = 0;
        for (size_t i = 0; i + 1 < route.size(); ++i) {
            length += Distance(route[i], route[i + 1]);
        }
        return length;
    }

    std::vector<int> Instance::GetCustomers() const {
        std::unordered_set<int> depot_set(depots.begin(), depots.end());
        std::vector<int> customers;
        customers.reserve(n);
        for (int v = 0; v < n; ++v) {
            if (!depot_set.contains(v)) customers.push_back(v);
        }
        return customers;
    }

} // namespace mdmtsp_minmax
