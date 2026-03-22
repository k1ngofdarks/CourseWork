#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace tsp::core {

struct ProgressEvent {
    std::string algorithm;
    double time_sec = 0.0;
    std::size_t iter = 0;
    double objective = 0.0;
    std::vector<int> route;
};

} // namespace tsp::core
