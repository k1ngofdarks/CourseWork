#include <solver.h>
#include <factory.h>
#include <logger.h>
#include <random>
#include <algorithm>

namespace tsp {

    class GreedyNearest : public Solver {
    public:
        int start = 0;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.count("start")) start = std::stoi(opts.at("start"));
            app::Logger::GetInstance().AddDebug("nearest configured: start=" + std::to_string(start));
        }

        void Solve(std::vector<int> &route) override {
            auto &logger = app::Logger::GetInstance();
            const Instance &inst = Instance::GetInstance();
            int n = inst.GetN();
            logger.AddInfo("nearest: start solve, n=" + std::to_string(n));
            route.clear();
            route.reserve(n + 1);
            std::vector<char> used(n, 0);
            int cur = std::clamp(start, 0, n - 1);
            route.push_back(cur);
            used[cur] = 1;
            for (int step = 1; step < n; ++step) {
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
            }
            route.push_back(route[0]);
            logger.AddInfo("nearest: finish solve, route_len=" + std::to_string(inst.RouteLength(route)));
        }
    };

// registration
    static bool reg = (SolverFactory::RegisterSolver("nearest", []() {
        return std::make_unique<GreedyNearest>();
    }), true);

} // namespace tsp
