#include <logger.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <vector>

int main(int argc, char **argv) {
    std::unordered_map<std::string, std::string> opts;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string key = argv[i];
        std::string value = argv[i + 1];
        if (key.rfind("--", 0) == 0) {
            key = key.substr(2);
        }
        opts[key] = value;
    }

    auto logger = tsp::CreateLoggerFromOptions(opts);
    auto stop_token = tsp::CreateStopTokenFromOptions(opts);

    tsp::SolverLogScope scope(logger, stop_token, "logger_smoke");
    std::vector<int> route = {0, 1, 2, 3, 0};

    for (int step = 0; step < 8; ++step) {
        if (scope.StopRequested()) {
            scope.Debug("stop requested in smoke test");
            break;
        }
        double current = 1000.0 - step * 20.0;
        if (step % 2 == 0) {
            current -= 15.0;
        }
        scope.ReportCandidate(route, current);
        scope.Debug("step=" + std::to_string(step) + " candidate=" + std::to_string(current));
        scope.TickPeriodic(route);
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    }

    std::cout << "logger_smoke_done";
    return 0;
}
