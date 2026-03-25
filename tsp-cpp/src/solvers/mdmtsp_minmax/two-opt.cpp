#include <solver.h>
#include <factory.h>
#include <logger.h>

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
            app::Logger::GetInstance().AddDebug("mdmtsp two-opt configured: time_limit=" + std::to_string(time_limit));
        }

        void Solve(std::vector<std::vector<int>> &routes) override {
            auto &logger = app::Logger::GetInstance();
            const auto &inst = Instance::GetInstance();
            auto start = std::chrono::high_resolution_clock::now();
            logger.AddInfo("mdmtsp two-opt: start solve, routes=" + std::to_string(routes.size()));
            for (std::vector<int> &route: routes){
                int n = static_cast<int>(route.size()) - 1;
                bool found_improvement = true;

                if (time_limit > 0 && static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::high_resolution_clock::now() - start).count()) > time_limit) {
                    logger.AddInfo("mdmtsp two-opt: time limit reached");
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
                            }
                        }
                    }
                }
            }
            double max_len = 0.0;
            for (const auto &route: routes) {
                max_len = std::max(max_len, inst.RouteLength(route));
            }
            logger.AddInfo("mdmtsp two-opt: finish solve, max_len=" + std::to_string(max_len));
        }
    };

    static bool reg = (SolverFactory::RegisterSolver("two-opt", []() {
        return std::make_unique<TwoOptSolver>();
    }), true);

} // namespace mdmtsp_minmax
