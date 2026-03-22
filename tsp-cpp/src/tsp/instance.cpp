#include <instance.h>
#include <sstream>
#include <iostream>
#include <cmath>
#include "../include/json.hpp"

namespace tsp {
    inline std::string ReadAllStdin() {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        return ss.str();
    }

    inline std::vector<std::vector<double>> CalculateHaversineDistances(const std::vector<double> &lat_deg,
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

    inline std::vector<std::vector<double>> CalculateEuclideanDistances(const std::vector<double> &x,
                                                                         const std::vector<double> &y) {
        size_t n = x.size();
        std::vector<std::vector<double>> dist(n, std::vector<double>(n));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                double dx = x[i] - x[j];
                double dy = y[i] - y[j];
                dist[i][j] = std::sqrt(dx * dx + dy * dy);
            }
        }
        return dist;
    }

    inline std::vector<std::vector<double>> ParseDistanceMatrix(const nlohmann::json &json) {
        size_t n = json["n"];
        std::vector<std::vector<double>> dist(n, std::vector<double>(n));
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j < n; ++j) {
                dist[i][j] = json["matrix"][i][j].get<double>();
            }
        }
        return dist;
    }

    inline std::vector<std::vector<double>> ParseCoordinatesAndBuild(const nlohmann::json &json,
                                                                      std::vector<double>& lat_out,
                                                                      std::vector<double>& lon_out) {
        size_t n = json["n"];
        std::vector<double> x(n), y(n);

        if (json.contains("coords")) {
            const nlohmann::json &row_x = json["coords"][0];
            const nlohmann::json &row_y = json["coords"][1];
            for (size_t i = 0; i < n; ++i) {
                x[i] = row_x[i].get<double>();
                y[i] = row_y[i].get<double>();
            }
        } else {
            const nlohmann::json &row_lat = json["latlon"][0];
            const nlohmann::json &row_lon = json["latlon"][1];
            for (size_t i = 0; i < n; ++i) {
                x[i] = row_lat[i].get<double>();
                y[i] = row_lon[i].get<double>();
            }
        }

        lat_out = x;
        lon_out = y;

        std::string metric = json.value("metric", "haversine");
        if (metric == "euclidean") {
            return CalculateEuclideanDistances(x, y);
        }
        return CalculateHaversineDistances(x, y);
    }

    Instance::Instance() {
        auto json = nlohmann::json::parse(ReadAllStdin());
        n = json["n"];

        std::string format = json.value("format", "coords");
        if (format == "matrix") {
            mat = ParseDistanceMatrix(json);
            latitudes.assign(n, 0.0);
            longitudes.assign(n, 0.0);
        } else {
            mat = ParseCoordinatesAndBuild(json, latitudes, longitudes);
        }
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
