#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "run_context.h"

namespace tsp::core {

class SolverBase {
public:
    virtual void Configure(const std::unordered_map<std::string, std::string>& opts) {}
    virtual void Solve(std::vector<int>& out) = 0;
    virtual void SetCallbacks(const RunCallbacks& callbacks) {}
    virtual ~SolverBase() = default;
};

using SolverCreator = std::function<std::unique_ptr<SolverBase>()>;

} // namespace tsp::core
