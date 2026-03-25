#include <solver.h>
#include <logger.h>
#include <factory.h>
#include <instance.h>

#include <algorithm>
#include <limits>
#include <random>

namespace mdmtsp_minmax {

    class RandomSolver : public Solver {
    public:
        int iter = 200;
        unsigned int seed = std::random_device{}();

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("iter")) iter = std::max(1, std::stoi(opts.at("iter")));
            if (opts.contains("seed")) seed = static_cast<unsigned int>(std::stoul(opts.at("seed")));
        }

        void Solve(std::vector<std::vector<int>> &routes) override {
            const auto &inst = Instance::GetInstance();
            tsp::SolverLogScope log_scope(logger_, stop_token_, "mdmtsp.random");
            const auto &depots = inst.GetDepots();
            auto customers = inst.GetCustomers();

            std::mt19937 rng(seed);

            double best_max = std::numeric_limits<double>::infinity();
            std::vector<std::vector<int>> best_routes;

            for (int it = 0; it < iter; ++it) {
                if (log_scope.StopRequested()) {
                    log_scope.Debug("stop requested");
                    break;
                }
                std::shuffle(customers.begin(), customers.end(), rng);

                std::vector<std::vector<int>> cur(depots.size());
                for (size_t r = 0; r < depots.size(); ++r) cur[r].push_back(depots[r]);

                for (size_t i = 0; i < customers.size(); ++i) {
                    cur[i % depots.size()].push_back(customers[i]);
                }

                double cur_max = 0;
                for (size_t r = 0; r < depots.size(); ++r) {
                    cur[r].push_back(depots[r]);
                    cur_max = std::max(cur_max, inst.RouteLength(cur[r]));
                }

                if (cur_max < best_max) {
                    best_max = cur_max;
                    best_routes = cur;
                    if (!best_routes.empty()) {
                        size_t idx = 0;
                        double max_len = 0;
                        for (size_t rid = 0; rid < best_routes.size(); ++rid) {
                            double len = inst.RouteLength(best_routes[rid]);
                            if (len > max_len) {
                                max_len = len;
                                idx = rid;
                            }
                        }
                        log_scope.ReportCandidate(best_routes[idx], max_len);
                    }
                }
                if (!cur.empty()) {
                    log_scope.TickPeriodic(cur.front());
                }
            }

            routes = std::move(best_routes);
        }
    };

    static bool reg = (SolverFactory::RegisterSolver("random", []() {
        return std::make_unique<RandomSolver>();
    }), true);

} // namespace mdmtsp_minmax
