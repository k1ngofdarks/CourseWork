#pragma once

#include "instance.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tsp {
    class ILogger;
    class IStopToken;

    class Solver {
    public:
        virtual void Configure(const std::unordered_map<std::string, std::string> &opts) {}
        virtual void SetLogger(std::shared_ptr<ILogger> logger) { logger_ = std::move(logger); }
        virtual void SetStopToken(std::shared_ptr<IStopToken> stop_token) { stop_token_ = std::move(stop_token); }
        virtual void Solve(std::vector<int> &out) = 0;
        virtual ~Solver() = default;

    protected:
        std::shared_ptr<ILogger> logger_;
        std::shared_ptr<IStopToken> stop_token_;
    };

    using SolverCreator = std::function<std::unique_ptr<Solver>()>;

} // namespace tsp

namespace mdmtsp_minmax {

    class Solver {
    public:
        virtual void Configure(const std::unordered_map<std::string, std::string> &opts) {}
        virtual void SetLogger(std::shared_ptr<tsp::ILogger> logger) { logger_ = std::move(logger); }
        virtual void SetStopToken(std::shared_ptr<tsp::IStopToken> stop_token) { stop_token_ = std::move(stop_token); }
        virtual void Solve(std::vector<std::vector<int>> &routes) = 0;
        virtual ~Solver() = default;

    protected:
        std::shared_ptr<tsp::ILogger> logger_;
        std::shared_ptr<tsp::IStopToken> stop_token_;
    };

    using SolverCreator = std::function<std::unique_ptr<Solver>()>;

} // namespace mdmtsp_minmax
