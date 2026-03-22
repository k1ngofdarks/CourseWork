#pragma once

#include <string>
#include <vector>

namespace tsp {
    class Instance {
    public:
        static Instance &GetInstance() {
            static Instance inst;
            return inst;
        }

        Instance(const Instance &) = delete;
        Instance &operator=(const Instance &) = delete;
        Instance(Instance &&) = delete;
        Instance &operator=(Instance &&) = delete;

        [[nodiscard]] int GetN() const;
        [[nodiscard]] double Distance(int i, int j) const;
        [[nodiscard]] double RouteLength(const std::vector<int> &route) const;

        [[nodiscard]] const std::vector<double> &GetLatitudes() const;
        [[nodiscard]] const std::vector<double> &GetLongitudes() const;

    private:
        Instance();
        ~Instance() = default;

        int n;
        std::vector<std::vector<double>> mat;
        std::vector<double> latitudes;
        std::vector<double> longitudes;
    };
} // namespace tsp

namespace mdmtsp_minmax {
    class Instance {
    public:
        static Instance &GetInstance() {
            static Instance inst;
            return inst;
        }

        Instance(const Instance &) = delete;
        Instance &operator=(const Instance &) = delete;
        Instance(Instance &&) = delete;
        Instance &operator=(Instance &&) = delete;

        [[nodiscard]] int GetN() const;
        [[nodiscard]] const std::vector<int> &GetDepots() const;
        [[nodiscard]] double Distance(int i, int j) const;
        [[nodiscard]] double RouteLength(const std::vector<int> &route) const;
        [[nodiscard]] std::vector<int> GetCustomers() const;

    private:
        Instance();
        ~Instance() = default;

        int n = 0;
        std::vector<std::vector<double>> mat;
        std::vector<int> depots;
    };
} // namespace mdmtsp_minmax
