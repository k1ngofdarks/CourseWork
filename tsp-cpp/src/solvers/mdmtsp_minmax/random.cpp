#include <solver.h>
#include <factory.h>
#include <instance.h>
#include <logger.h>

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
            app::Logger::GetInstance().AddDebug(
                    "mdmtsp random configured: iter=" + std::to_string(iter) +
                    ", seed=" + std::to_string(seed));
        }

        void Solve(std::vector<std::vector<int>> &routes) override {
            auto &logger = app::Logger::GetInstance();
            const auto &inst = Instance::GetInstance();
            const auto &depots = inst.GetDepots();
            auto customers = inst.GetCustomers();

            std::mt19937 rng(seed);

            double best_max = std::numeric_limits<double>::infinity();
            std::vector<std::vector<int>> best_routes;
            logger.AddInfo("mdmtsp random: start solve, iter=" + std::to_string(iter));

            for (int it = 0; it < iter; ++it) {
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
                    logger.AddDebug("mdmtsp random: improved max_len=" + std::to_string(best_max) +
                                    ", iter=" + std::to_string(it + 1));
                }
            }

            routes = std::move(best_routes);
            logger.AddInfo("mdmtsp random: finish solve, best_max=" + std::to_string(best_max));
        }
    };

    static bool reg = (SolverFactory::RegisterSolver("random", []() {
        return std::make_unique<RandomSolver>();
    }), true);

} // namespace mdmtsp_minmax
