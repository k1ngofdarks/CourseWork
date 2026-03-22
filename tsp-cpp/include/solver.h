#pragma once
#include "instance.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>

namespace tsp {

    class Solver {
    public:
        // Used to pass arguments inside the solver
        virtual void Configure(const std::unordered_map<std::string,std::string>& opts) {}
        virtual void Solve(std::vector<int>& out) = 0;
        virtual ~Solver() = default;
    };

    using SolverCreator = std::function<std::unique_ptr<Solver>()>;

} // namespace tsp