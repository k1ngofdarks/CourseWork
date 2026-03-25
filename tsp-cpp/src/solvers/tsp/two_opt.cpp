#include <solver.h>
#include <logger.h>
#include <factory.h>
#include <random>
#include <algorithm>
#include <chrono>

namespace tsp {

    class TwoOpt : public Solver { // NOLINT
    public:
        double time_limit = -1;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("time")) {
                time_limit = std::stod(opts.at("time"));
            }
        }

        void Solve(std::vector<int> &route) override {
            const Instance &inst = Instance::GetInstance();
            int n = inst.GetN();
            auto start = std::chrono::high_resolution_clock::now();
            SolverLogScope log_scope(logger_, stop_token_, "2-opt");
            log_scope.ReportCandidate(route, CalculateRouteLength(route));
            bool found_improvement = true;
            while (found_improvement) {
                if (log_scope.StopRequested()) {
                    log_scope.Debug("stop requested");
                    return;
                }
                found_improvement = false;
                for (int i = 0; i < n - 1; ++i) {
                    if (time_limit > 0 && static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::high_resolution_clock::now() - start).count()) > time_limit) {
                        return;
                    }
                    for (int j = i + 2; j < n; ++j) {
                        if (inst.Distance(route[i], route[i + 1]) + inst.Distance(route[j], route[j + 1]) >
                            inst.Distance(route[i], route[j]) + inst.Distance(route[i + 1], route[j + 1])) {
                            SwapPath(route, i + 1, j);
                            found_improvement = true;
                            log_scope.ReportCandidate(route, CalculateRouteLength(route));
                        }
                    }
                }
                log_scope.TickPeriodic(route);
            }
        }

    private:
        static void SwapPath(std::vector<int> &route, int i, int j) {
            for (; i < j; i++, j--) {
                std::swap(route[i], route[j]);
            }
        }
    };

// registration
    static bool reg = (SolverFactory::RegisterSolver("2-opt", []() {
        return std::make_unique<TwoOpt>();
    }), true);

} // namespace tsp
