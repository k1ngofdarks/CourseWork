#include <solver.h>
#include <factory.h>
#include <logger.h>

#include <algorithm>
#include <vector>
#include <limits>

namespace mdmtsp_minmax {

    class NearestNeighborSolver : public Solver {
    public:
        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            app::Logger::GetInstance().AddDebug("mdmtsp nn configured");
        }

        void Solve(std::vector<std::vector<int>> &routes) override {
            auto &logger = app::Logger::GetInstance();
            const auto &inst = Instance::GetInstance();
            const auto &depots = inst.GetDepots();
            auto customers = inst.GetCustomers();

            int n_depots = static_cast<int>(depots.size());

            std::vector<std::vector<int>> best_routes(n_depots);
            std::vector<long double> route_lengths(n_depots, 0.0);

            for (int i = 0; i < n_depots; ++i) {
                best_routes[i].push_back(depots[i]);
            }

            std::vector<int> not_visited(customers.begin(), customers.end());
            logger.AddInfo("mdmtsp nn: start solve, customers=" + std::to_string(not_visited.size()));

            while (!not_visited.empty()) {
                int best_route_id =
                        static_cast<int>(std::min_element(route_lengths.begin(), route_lengths.end()) - route_lengths.begin());
                int curr_node = best_routes[best_route_id].back();

                int best_customer_id = 0;
                long double min_dist = 1e18;

                for (int i = 0; i < not_visited.size(); ++i) {
                    long double dist = inst.Distance(curr_node, not_visited[i]);
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_customer_id = i;
                    }
                }

                int next_node = not_visited[best_customer_id];
                best_routes[best_route_id].push_back(next_node);
                route_lengths[best_route_id] += min_dist + inst.Distance(best_customer_id, depots[best_route_id]) -
                                                inst.Distance(depots[best_route_id], curr_node);

                std::swap(not_visited[best_customer_id], not_visited.back());
                not_visited.pop_back();
            }

            for (int i = 0; i < n_depots; ++i) {
                best_routes[i].push_back(best_routes[i][0]);
            }

            std::swap(routes, best_routes);
            std::vector<double> route_lens;
            double max_len = 0.0;
            for (const auto &route: routes) {
                double len = inst.RouteLength(route);
                route_lens.push_back(len);
                max_len = std::max(max_len, len);
            }
            logger.AddInfo("mdmtsp nn: finish solve, max_len=" + std::to_string(max_len));
        }
    };

    static bool reg_nn = (SolverFactory::RegisterSolver("nn", []() {
        return std::make_unique<NearestNeighborSolver>();
    }), true);

} // namespace mdmtsp_minmax
