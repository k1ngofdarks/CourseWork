#include "tsp/solver.h"
#include "tsp/factory.h"
#include <random>
#include <chrono>
#include <exception>
#include "tsp/instance.h"

namespace tsp {
    class ILS : public Solver { // NOLINT
    public:
        double time_limit = -1;
        uint64_t cnt_pert = 1;
        size_t max_iter = 0;
        std::mt19937 rnd;
        SolverCallbacks callbacks;


        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("time")) {
                time_limit = std::stod(opts.at("time"));
            }
            if (opts.contains("cnt_pert")) {
                cnt_pert = std::stoull(opts.at("cnt_pert"));
            }
            if (opts.contains("max_iter")) {
                max_iter = std::stoull(opts.at("max_iter"));
            }
            if (max_iter <= 0 && time_limit <= 0) {
                throw std::invalid_argument("At least one of the parameters time or cnt_iter must be present");
            }
        }

        void SetCallbacks(const SolverCallbacks &cb) override { callbacks = cb; }

        void Solve(std::vector<int> &route) override {
            const Instance &inst = Instance::GetInstance();
            auto start = std::chrono::high_resolution_clock::now();

            auto nearest = tsp::SolverFactory::Create("nearest");
            auto opt_2 = tsp::SolverFactory::CreateConfigured("2-opt",
                                                              {std::make_pair("time", std::to_string(time_limit))});
            opt_2->SetCallbacks(callbacks);
            nearest->Solve(route);
            opt_2->Solve(route);
            for (size_t iter_id = 0; (iter_id < max_iter || max_iter == 0) &&
                                     (ElapsedTime(start) < time_limit || time_limit <= 0); ++iter_id) {
                if (callbacks.should_stop && callbacks.should_stop()) break;
                std::vector<int> new_route = route;
                KDoubleBridgeMove(new_route, cnt_pert);

                opt_2->Solve(new_route);
                if (inst.RouteLength(new_route) <= inst.RouteLength(route)) {
                    route = new_route;
                    if (callbacks.on_progress) callbacks.on_progress(route, iter_id + 1);
                }
            }
        }

    private:
        template<typename T>
        static double ElapsedTime(T start) {
            return static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now() - start).count());
        }

        void KDoubleBridgeMove(std::vector<int> &route, uint64_t k) {
            int n = static_cast<int>(route.size()) - 1;
            for (uint64_t iter_id = 0; iter_id < k; ++iter_id) {
                int i = static_cast<int>(rnd() % (n - 3)) + 1;
                int j = static_cast<int>(rnd() % (n - i - 2)) + i + 1;
                int h = static_cast<int>(rnd() % (n - j - 1)) + j + 1;
                std::vector<int> tmp(j - i);
                for (int pos = i; pos < j; ++pos) {
                    tmp[pos - i] = route[pos];
                }
                for (int pos = j; pos < h; ++pos) {
                    route[pos - j + i] = route[pos];
                }
                for (int pos = 0; pos < j - i; ++pos) {
                    route[h - j + i + pos] = tmp[pos];
                }
            }
        }
    };

    static bool reg = (SolverFactory::RegisterSolver("ils", []() {
        return std::make_unique<ILS>();
    }), true);

}
