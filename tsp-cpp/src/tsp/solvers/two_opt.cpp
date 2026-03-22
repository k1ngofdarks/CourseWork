#include "tsp/solver.h"
#include "tsp/factory.h"
#include <random>
#include <algorithm>
#include <chrono>
#include "tsp/instance.h"

namespace tsp {

    class TwoOpt : public Solver { // NOLINT
    public:
        double time_limit = -1;
        SolverCallbacks callbacks;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("time")) {
                time_limit = std::stod(opts.at("time"));
            }
        }

        void SetCallbacks(const SolverCallbacks &cb) override { callbacks = cb; }

        void Solve(std::vector<int> &route) override {
            const Instance &inst = Instance::GetInstance();
            int n = inst.GetN();
            auto start = std::chrono::high_resolution_clock::now();
            bool found_improvement = true;
            std::size_t iter = 0;
            while (found_improvement) {
                found_improvement = false;
                for (int i = 0; i < n - 1; ++i) {
                    if (callbacks.should_stop && callbacks.should_stop()) return;
                    if (time_limit > 0 && static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::high_resolution_clock::now() - start).count()) > time_limit) {
                        return;
                    }
                    for (int j = i + 2; j < n; ++j) {
                        if (inst.Distance(route[i], route[i + 1]) + inst.Distance(route[j], route[j + 1]) >
                            inst.Distance(route[i], route[j]) + inst.Distance(route[i + 1], route[j + 1])) {
                            SwapPath(route, i + 1, j);
                            found_improvement = true;
                            ++iter;
                            if (callbacks.on_progress) callbacks.on_progress(route, iter);
                        }
                    }
                }
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
