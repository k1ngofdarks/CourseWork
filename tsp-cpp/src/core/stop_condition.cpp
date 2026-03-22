#include "core/stop_condition.h"

namespace tsp::core {

volatile std::sig_atomic_t g_stop_signal = 0;

void HandleSignal(int) {
    g_stop_signal = 1;
}

StopCondition::StopCondition(double run_time_limit_sec)
    : run_time_limit_sec_(run_time_limit_sec), start_(std::chrono::high_resolution_clock::now()) {}

bool StopCondition::ShouldStop() const {
    if (g_stop_signal) return true;
    if (run_time_limit_sec_ <= 0) return false;
    double elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - start_).count()) / 1e3;
    return elapsed >= run_time_limit_sec_;
}

void StopCondition::InstallSignalHandlers() {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
}

bool StopCondition::IsSignalStopRequested() {
    return g_stop_signal != 0;
}

} // namespace tsp::core
