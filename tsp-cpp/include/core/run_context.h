#pragma once

#include <functional>
#include <vector>
#include <cstddef>

namespace tsp::core {

struct RunCallbacks {
    std::function<bool()> should_stop;
    std::function<void(const std::vector<int>& route, std::size_t iter)> on_progress;
};

struct RunOptions {
    double run_time_limit = -1;
    double checkpoint_every_sec = 30.0;
};

} // namespace tsp::core
