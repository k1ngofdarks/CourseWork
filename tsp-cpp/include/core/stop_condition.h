#pragma once

#include <chrono>
#include <csignal>

namespace tsp::core {

class StopCondition {
public:
    explicit StopCondition(double run_time_limit_sec);

    bool ShouldStop() const;

    static void InstallSignalHandlers();
    static bool IsSignalStopRequested();

private:
    double run_time_limit_sec_;
    std::chrono::high_resolution_clock::time_point start_;
};

} // namespace tsp::core
