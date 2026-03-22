#include <instance.h>
#include <sstream>
#include <iostream>
#include "../include/json.hpp"

namespace tsp {
    inline std::string ReadAllStdin() {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        return ss.str();
    }

    inline std::vector<std::vector<double>>
    CalculateDistances(const std::vector<double> &lat_deg, const std::vector<double> &lon_deg) {
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

    inline std::vector<std::vector<double>> ParseCalculateDistances(const nlohmann::json &json,
                                                                   std::vector<double>& lat_out,
                                                                   std::vector<double>& lon_out) {
        size_t n = json["n"];
        std::vector<double> lat(n), lon(n);
        const nlohmann::json &row_lat = json["latlon"][0];
        for (size_t i = 0; i < n; ++i) {
            lat[i] = row_lat[i].get<double>();
        }
        const nlohmann::json &row_lon = json["latlon"][1];
        for (size_t i = 0; i < n; ++i) {
            lon[i] = row_lon[i].get<double>();
        }

        // Store coordinates for external access
        lat_out = lat;
        lon_out = lon;

        return CalculateDistances(lat, lon);
    }

    Instance::Instance() {
        auto json = nlohmann::json::parse(ReadAllStdin());
        n = json["n"];
        mat = ParseCalculateDistances(json, latitudes, longitudes);
    }

    int Instance::GetN() const {
        return n;
    }

    double Instance::Distance(int i, int j) const {
        return mat[i][j];
    }

    double Instance::RouteLength(const std::vector<int> &route) const {
        double length = 0;
        for (size_t i = 0; i + 1 < route.size(); ++i) {
            length += Distance(route[i], route[i + 1]);
        }
        return length;
    }

    const std::vector<double>& Instance::GetLatitudes() const {
        return latitudes;
    }

    const std::vector<double>& Instance::GetLongitudes() const {
        return longitudes;
    }

} // namespace tsp