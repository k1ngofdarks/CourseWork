#include <solver.h>
#include <logger.h>
#include <factory.h>

#include <algorithm>
#include <vector>
#include <limits>

namespace mdmtsp_minmax {

    class NearestNeighborSolver : public Solver {
    public:
        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
        }

        void Solve(std::vector<std::vector<int>> &routes) override {
            const auto &inst = Instance::GetInstance();
            tsp::SolverLogScope log_scope(logger_, stop_token_, "mdmtsp.nn", -1.0, true, debug_logging_enabled_);
            const auto &depots = inst.GetDepots();
            auto customers = inst.GetCustomers();

            int n_depots = static_cast<int>(depots.size());

            std::vector<std::vector<int>> best_routes(n_depots);
            std::vector<long double> route_lengths(n_depots, 0.0);

            for (int i = 0; i < n_depots; ++i) {
                best_routes[i].push_back(depots[i]);
            }

            std::vector<int> not_visited(customers.begin(), customers.end());

            while (!not_visited.empty()) {
                if (log_scope.StopRequested()) {
                    log_scope.Debug("stop requested");
                    return;
                }
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
                auto it = std::max_element(route_lengths.begin(), route_lengths.end());
                if (it != route_lengths.end()) {
                    log_scope.ReportCandidate(best_routes[static_cast<size_t>(it - route_lengths.begin())],
                                              static_cast<double>(*it));
                }
                log_scope.TickPeriodic(best_routes[best_route_id]);
            }

            for (int i = 0; i < n_depots; ++i) {
                best_routes[i].push_back(best_routes[i][0]);
            }
            long double best_max = 0.0;
            size_t best_idx = 0;
            for (size_t i = 0; i < best_routes.size(); ++i) {
                long double len = inst.RouteLength(best_routes[i]);
                if (len > best_max) {
                    best_max = len;
                    best_idx = i;
                }
            }
            if (!best_routes.empty()) {
                log_scope.ReportCandidate(best_routes[best_idx], static_cast<double>(best_max));
            }

            std::swap(routes, best_routes);
        }
    };

    static bool reg_nn = (SolverFactory::RegisterSolver("nn", []() {
        return std::make_unique<NearestNeighborSolver>();
    }), true);

} // namespace mdmtsp_minmax
