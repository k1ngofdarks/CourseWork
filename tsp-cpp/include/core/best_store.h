#pragma once

#include <vector>
#include <string>
#include <optional>
#include <limits>

namespace tsp::core {

class BestStore {
public:
    bool TryUpdate(const std::vector<int>& route, double objective, double time_sec, std::size_t iter,
                   const std::string& algorithm);

    [[nodiscard]] bool HasValue() const { return route_.has_value(); }
    [[nodiscard]] const std::vector<int>& Route() const { return route_.value(); }
    [[nodiscard]] double Objective() const { return objective_; }
    [[nodiscard]] double TimeSec() const { return time_sec_; }
    [[nodiscard]] std::size_t Iter() const { return iter_; }
    [[nodiscard]] const std::string& Algorithm() const { return algorithm_; }

private:
    std::optional<std::vector<int>> route_;
    double objective_ = std::numeric_limits<double>::infinity();
    double time_sec_ = 0.0;
    std::size_t iter_ = 0;
    std::string algorithm_;
};

} // namespace tsp::core
