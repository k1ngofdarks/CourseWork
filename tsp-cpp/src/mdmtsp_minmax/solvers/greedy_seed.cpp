#include "mdmtsp_minmax/mdmtsp_solver_api.h"
#include <algorithm>

namespace mdmtsp_minmax {

Solution SolveGreedySeed(const Instance& inst) {
    Solution sol;
    auto route_depots = inst.ExpandedDepotVehicles();
    if (route_depots.empty()) return sol;

    const int n_routes = static_cast<int>(route_depots.size());
    sol.routes.resize(n_routes);
    std::vector<double> lengths(n_routes, 0.0);
    for (int r = 0; r < n_routes; ++r) {
        sol.routes[r].depot_id = route_depots[r];
    }

    std::vector<int> customers = inst.Customers();
    for (int c : customers) {
        int best_r = 0;
        double best_obj = 1e300;

        for (int r = 0; r < n_routes; ++r) {
            const int depot = sol.routes[r].depot_id;
            double new_len = inst.RouteLength(depot, sol.routes[r].nodes);
            std::vector<int> candidate = sol.routes[r].nodes;
            candidate.push_back(c);
            new_len = inst.RouteLength(depot, candidate);

            double obj = 0.0;
            for (int rr = 0; rr < n_routes; ++rr) {
                if (rr == r) obj = std::max(obj, new_len);
                else obj = std::max(obj, lengths[rr]);
            }

            if (obj < best_obj) {
                best_obj = obj;
                best_r = r;
            }
        }

        sol.routes[best_r].nodes.push_back(c);
        lengths[best_r] = inst.RouteLength(sol.routes[best_r].depot_id, sol.routes[best_r].nodes);
    }

    sol.max_route_length = 0.0;
    for (int r = 0; r < n_routes; ++r) {
        sol.routes[r].length = inst.RouteLength(sol.routes[r].depot_id, sol.routes[r].nodes);
        sol.max_route_length = std::max(sol.max_route_length, sol.routes[r].length);
    }
    return sol;
}

} // namespace mdmtsp_minmax
