#include <solver.h>
#include <logger.h>
#include <factory.h>
#include <random>
#include <chrono>
#include <exception>

namespace tsp {
    class GLS : public Solver { // NOLINT
    public:
        double time_limit = -1;
        size_t max_iter = 0;
        double lambda = 0.2;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("time")) {
                time_limit = std::stod(opts.at("time"));
            }
            if (opts.contains("max_iter")) {
                max_iter = std::stoull(opts.at("max_iter"));
            }
            if (opts.contains("lambda")) {
                lambda = std::stod(opts.at("lambda"));
            }
            if (max_iter <= 0 && time_limit <= 0) {
                throw std::invalid_argument("At least one of the parameters time or cnt_iter must be present");
            }
        }

        void Solve(std::vector<int> &route) override {
            const Instance &inst = Instance::GetInstance();
            auto start = std::chrono::high_resolution_clock::now();
            SolverLogScope log_scope(logger_, stop_token_, "gls", -1.0, true, debug_logging_enabled_);

            auto nearest = tsp::SolverFactory::Create("nearest");
            nearest->SetLogger(logger_);
            nearest->SetStopToken(stop_token_);
            nearest->SetDebugLoggingEnabled(false);
            nearest->Solve(route);
            int n = static_cast<int>(route.size()) - 1;
            std::vector<std::vector<int>> penalties(n, std::vector<int>(n, 0));
            std::vector<int> best_route = route;
            log_scope.ReportCandidate(best_route, inst.RouteLength(best_route));
            for (size_t iter_id = 0; (iter_id < max_iter || max_iter == 0) &&
                                     (ElapsedTime(start) < time_limit || time_limit <= 0); ++iter_id) {
                if (log_scope.StopRequested()) {
                    log_scope.Debug("stop requested");
                    break;
                }
                ImprovedTwoOpt(route, penalties);
                if (inst.RouteLength(route) <= inst.RouteLength(best_route)) {
                    best_route = route;
                    log_scope.ReportCandidate(best_route, inst.RouteLength(best_route));
                }
                double max_utility = 0;
                for (size_t i = 0; i < n; i++) {
                    double utility = inst.Distance(route[i], route[i + 1]) / (1 + penalties[route[i]][route[i + 1]]);
                    max_utility = std::max(max_utility, utility);
                }

                double eps = 1e-8;
                for (size_t i = 0; i < n; i++) {
                    double utility = inst.Distance(route[i], route[i + 1]) / (1 + penalties[route[i]][route[i + 1]]);
                    if (utility > max_utility - eps) {
                        ++penalties[route[i]][route[i + 1]];
                    }
                }
                log_scope.TickPeriodic(best_route);
            }
            route = best_route;
        }

    private:
        template<typename T>
        static double ElapsedTime(T start) {
            return static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now() - start).count());
        }

        void ImprovedTwoOpt(std::vector<int> &route, const std::vector<std::vector<int>> &penalties) const {
            const Instance &inst = Instance::GetInstance();
            int n = inst.GetN();
            auto start = std::chrono::high_resolution_clock::now();
            bool found_improvement = true;
            auto improve_dist = [&penalties, &inst, this](int i, int j) {
                return inst.Distance(i, j) + lambda * penalties[i][j];
            };
            while (found_improvement) {
                found_improvement = false;
                for (int i = 0; i < n - 1; ++i) {
                    for (int j = i + 2; j < n; ++j) {
                        if (improve_dist(route[i], route[i + 1]) + improve_dist(route[j], route[j + 1]) >
                            improve_dist(route[i], route[j]) + improve_dist(route[i + 1], route[j + 1])) {
                            SwapPath(route, i + 1, j);
                            found_improvement = true;
                        }
                    }
                }
            }
        }

        static void SwapPath(std::vector<int> &route, int i, int j) {
            for (; i < j; i++, j--) {
                std::swap(route[i], route[j]);
            }
        }

    };

    static bool reg = (SolverFactory::RegisterSolver("gls", []() {
        return std::make_unique<GLS>();
    }), true);

}
