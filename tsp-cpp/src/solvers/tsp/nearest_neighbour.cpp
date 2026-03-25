#include <solver.h>
#include <logger.h>
#include <factory.h>
#include <random>
#include <algorithm>

namespace tsp {

    class GreedyNearest : public Solver {
    public:
        int start = 0;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.count("start")) start = std::stoi(opts.at("start"));
        }

        void Solve(std::vector<int> &route) override {
            const Instance &inst = Instance::GetInstance();
            int n = inst.GetN();
            SolverLogScope log_scope(logger_, stop_token_, "nearest", -1.0, true, debug_logging_enabled_);
            std::vector<int> old_route = route;
            route.clear();
            route.reserve(n + 1);
            std::vector<char> used(n, 0);
            int cur = std::clamp(start, 0, n - 1);
            route.push_back(cur);
            used[cur] = 1;
            for (int step = 1; step < n; ++step) {
                if (log_scope.StopRequested()) {
                    log_scope.Debug("stop requested");
                    route = old_route;
                    return;
                }
                int best = -1;
                double bestd = 1e300;
                for (int j = 0; j < n; ++j)
                    if (!used[j]) {
                        double d = inst.Distance(cur, j);
                        if (d < bestd) {
                            bestd = d;
                            best = j;
                        }
                    }
                route.push_back(best);
                used[best] = 1;
                cur = best;
                log_scope.TickPeriodic(route);
            }
            route.push_back(route[0]);
            log_scope.ReportCandidate(route, CalculateRouteLength(route));
        }
    };

// registration
    static bool reg = (SolverFactory::RegisterSolver("nearest", []() {
        return std::make_unique<GreedyNearest>();
    }), true);

} // namespace tsp
