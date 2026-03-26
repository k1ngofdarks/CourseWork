#include <solver.h>
#include <factory.h>
#include <logger.h>
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
            app::Logger::GetInstance().AddDebug("2-opt configured: time_limit=" + std::to_string(time_limit));
        }

        void Solve(std::vector<int> &route) override {
            auto &logger = app::Logger::GetInstance();
            const Instance &inst = Instance::GetInstance();
            int n = inst.GetN();
            logger.AddInfo("2-opt: start solve, n=" + std::to_string(n));
            auto start = std::chrono::high_resolution_clock::now();
            bool found_improvement = true;
            double best_len = inst.RouteLength(route);
            while (found_improvement) {
                found_improvement = false;
                for (int i = 0; i < n - 1; ++i) {
                    if (time_limit > 0 && static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::high_resolution_clock::now() - start).count()) > time_limit) {
                        logger.AddInfo("2-opt: time limit reached, best_len=" + std::to_string(best_len));
                        return;
                    }
                    for (int j = i + 2; j < n; ++j) {
                        if (inst.Distance(route[i], route[i + 1]) + inst.Distance(route[j], route[j + 1]) >
                            inst.Distance(route[i], route[j]) + inst.Distance(route[i + 1], route[j + 1])) {
                            SwapPath(route, i + 1, j);
                            found_improvement = true;
                            const double cur_len = inst.RouteLength(route);
                            if (cur_len < best_len) {
                                best_len = cur_len;
                                logger.AddDebug("2-opt: improved route_len=" + std::to_string(best_len));
                            }
                        }
                    }
                }
            }
            logger.AddInfo("2-opt: finish solve, route_len=" + std::to_string(best_len));
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
