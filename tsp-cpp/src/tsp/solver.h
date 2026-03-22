#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "progress.h"

namespace tsp {

class Solver {
public:
    virtual void Configure(const std::unordered_map<std::string, std::string>& opts) {}
    virtual void Solve(std::vector<int>& out) = 0;
    virtual void SetCallbacks(const SolverCallbacks& callbacks) {}
    virtual ~Solver() = default;
};

using SolverCreator = std::function<std::unique_ptr<Solver>()>;

} // namespace tsp
