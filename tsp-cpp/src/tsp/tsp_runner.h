#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace tsp {

struct StepConfig {
    std::string name;
    std::unordered_map<std::string, std::string> args;
};

struct RunnerInput {
    std::unordered_map<std::string, std::string> global_opts;
    std::vector<StepConfig> steps;
};

std::string RunTsp(const RunnerInput& input);

} // namespace tsp
