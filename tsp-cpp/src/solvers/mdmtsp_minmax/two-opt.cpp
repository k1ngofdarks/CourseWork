#include <solver.h>
#include <logger.h>
#include <factory.h>

#include <algorithm>
#include <random>
#include <chrono>

namespace mdmtsp_minmax {

    class TwoOptSolver : public Solver {
    public:
        double time_limit = -1;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("time")) {
                time_limit = std::stod(opts.at("time"));
            }
        }

        void Solve(std::vector<std::vector<int>> &routes) override {
            const auto &inst = Instance::GetInstance();
            tsp::SolverLogScope log_scope(logger_, stop_token_, "mdmtsp.two-opt", -1.0, true, debug_logging_enabled_);
            auto start = std::chrono::high_resolution_clock::now();
            auto report_routes = [&]() {
                double best = 0;
                size_t idx = 0;
                for (size_t rid = 0; rid < routes.size(); ++rid) {
                    double len = inst.RouteLength(routes[rid]);
                    if (len > best) {
                        best = len;
                        idx = rid;
                    }
                }
                if (!routes.empty()) {
                    log_scope.ReportCandidate(routes[idx], best);
                }
            };
            report_routes();
            for (std::vector<int> &route: routes){
                int n = static_cast<int>(route.size()) - 1;
                bool found_improvement = true;

                if (log_scope.StopRequested()) {
                    log_scope.Debug("stop requested");
                    return;
                }
                if (time_limit > 0 && static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::high_resolution_clock::now() - start).count()) > time_limit) {
                    return;
                }

                while (found_improvement) {
                    found_improvement = false;
                    for (int i = 0; i < n - 1; ++i) {
                        for (int j = i + 2; j < n; ++j) {
                            if (inst.Distance(route[i], route[i + 1]) + inst.Distance(route[j], route[j + 1]) >
                                inst.Distance(route[i], route[j]) + inst.Distance(route[i + 1], route[j + 1])) {
                                std::reverse(route.begin() + i + 1, route.begin() + j + 1);
                                found_improvement = true;
                                report_routes();
                            }
                        }
                    }
                    log_scope.TickPeriodic(route);
                }
            }
        }
    };

    static bool reg = (SolverFactory::RegisterSolver("two-opt", []() {
        return std::make_unique<TwoOptSolver>();
    }), true);

} // namespace mdmtsp_minmax
