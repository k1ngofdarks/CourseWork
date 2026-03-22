#pragma once

#include <vector>
#include <string>
#include <functional>
#include <cstddef>

namespace tsp {

struct SolverCallbacks {
    std::function<bool()> should_stop;
    std::function<void(const std::vector<int>& route, std::size_t iter)> on_progress;
};

} // namespace tsp
