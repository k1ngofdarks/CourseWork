#include <solver.h>
#include <factory.h>
#include <logger.h>
#include <algorithm>
#include <limits>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <string>

namespace tsp {

class HeldKarp : public Solver {
public:
    int start = 0;
    int max_n = 20; // если вершин больше, алгоритм не запускается

    void Configure(const std::unordered_map<std::string, std::string> &opts) override {
        if (opts.count("start")) start = std::stoi(opts.at("start"));
        if (opts.count("max_n")) max_n = std::stoi(opts.at("max_n"));
        app::Logger::GetInstance().AddDebug(
                "heldkarp configured: start=" + std::to_string(start) +
                ", max_n=" + std::to_string(max_n));
    }

    void Solve(std::vector<int> &route) override {
        auto &logger = app::Logger::GetInstance();
        const Instance &inst = Instance::GetInstance();
        const int n = inst.GetN();
        logger.AddInfo("heldkarp: start solve, n=" + std::to_string(n));

        if (n <= 0 || n > max_n) {
            logger.AddInfo("heldkarp: skipped, n=" + std::to_string(n) + ", max_n=" + std::to_string(max_n));
            return;
        }

        const int s = std::clamp(start, 0, n - 1);
        const uint32_t FULL = (1u << n);
        const double INF = std::numeric_limits<double>::infinity();

        std::vector<double> dp(static_cast<size_t>(FULL) * n, INF);
        std::vector<int> parent(static_cast<size_t>(FULL) * n, -1);

        auto idx = [n](uint32_t mask, int j) -> size_t {
            return static_cast<size_t>(mask) * n + static_cast<size_t>(j);
        };

        dp[idx(1u << s, s)] = 0.0;

        for (uint32_t mask = 0; mask < FULL; ++mask) {
            if (((mask >> s) & 1u) == 0u) continue;
            for (int j = 0; j < n; ++j) {
                if (((mask >> j) & 1u) == 0u) continue;
                double cur = dp[idx(mask, j)];
                if (cur == INF) continue;

                for (int k = 0; k < n; ++k) {
                    if ((mask >> k) & 1u) continue;
                    uint32_t nmask = mask | (1u << k);
                    double cand = cur + inst.Distance(j, k);
                    size_t to = idx(nmask, k);
                    if (cand < dp[to]) {
                        dp[to] = cand;
                        parent[to] = j;
                    }
                }
            }
        }

        const uint32_t all = FULL - 1u;
        double best = INF;
        int best_end = -1;

        for (int j = 0; j < n; ++j) {
            if (j == s) continue;
            double val = dp[idx(all, j)];
            if (val == INF) continue;
            val += inst.Distance(j, s);
            if (val < best) {
                best = val;
                best_end = j;
            }
        }

        if (best_end == -1) return;

        std::vector<int> rev;
        rev.reserve(n);
        uint32_t mask = all;
        int cur = best_end;

        while (cur != s) {
            rev.push_back(cur);
            int p = parent[idx(mask, cur)];
            mask ^= (1u << cur);
            cur = p;
            if (cur < 0) return;
        }

        std::vector<int> new_route;
        new_route.reserve(n + 1);
        new_route.push_back(s);
        for (auto it = rev.rbegin(); it != rev.rend(); ++it)
            new_route.push_back(*it);
        new_route.push_back(s);

        route.swap(new_route);
        logger.AddInfo("heldkarp: finish solve, route_len=" + std::to_string(inst.RouteLength(route)));
    }
};

// registration
static bool reg = (SolverFactory::RegisterSolver("heldkarp", []() {
    return std::make_unique<HeldKarp>();
}), true);

} // namespace tsp
