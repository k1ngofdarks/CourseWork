#include "mdmtsp_minmax/mdmtsp_solver_api.h"
#include <random>
#include <algorithm>

namespace mdmtsp_minmax {

namespace {

Solution BuildRandomSolution(const Instance& inst, std::mt19937& rng) {
    Solution sol;
    if (inst.k_vehicles <= 0 || inst.depots.empty()) return sol;

    sol.routes.resize(inst.k_vehicles);
    for (int r = 0; r < inst.k_vehicles; ++r) {
        sol.routes[r].depot_id = inst.depots[r % static_cast<int>(inst.depots.size())];
    }

    auto customers = inst.Customers();
    std::uniform_int_distribution<int> route_pick(0, inst.k_vehicles - 1);
    for (int c : customers) {
        int r = route_pick(rng);
        sol.routes[r].nodes.push_back(c);
    }

    sol.max_route_length = 0.0;
    for (auto& route : sol.routes) {
        route.length = inst.RouteLength(route.depot_id, route.nodes);
        sol.max_route_length = std::max(sol.max_route_length, route.length);
    }
    return sol;
}

} // namespace

Solution SolveRandomTemplate(const Instance& inst, int iterations, unsigned seed) {
    std::mt19937 rng(seed);
    iterations = std::max(1, iterations);

    Solution best;
    best.max_route_length = 1e300;

    for (int it = 0; it < iterations; ++it) {
        Solution cur = BuildRandomSolution(inst, rng);
        if (cur.max_route_length < best.max_route_length) {
            best = std::move(cur);
        }
    }
    if (best.max_route_length > 1e299) best.max_route_length = 0.0;
    return best;
}

} // namespace mdmtsp_minmax
