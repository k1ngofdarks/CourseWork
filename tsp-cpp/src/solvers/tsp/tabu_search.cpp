#include <solver.h>
#include <factory.h>
#include <logger.h>
#include <random>
#include <chrono>
#include <exception>
#include <algorithm>

namespace tsp {
    class TabuSearch : public Solver { // NOLINT
    public:
        double time_limit = -1;
        size_t max_iter = 0;
        std::mt19937 rnd;
        int k_opt = 3;
        int tenure = -1;
        int neighborhood_size = 500;
        int look_ahead = 40;
        double max_worsen_ratio = 0.03;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("time")) time_limit = std::stod(opts.at("time"));
            if (opts.contains("max_iter")) max_iter = std::stoull(opts.at("max_iter"));
            if (max_iter <= 0 && time_limit <= 0) {
                throw std::invalid_argument("At least one of the parameters time or cnt_iter must be present");
            }
            if (opts.contains("tenure")) tenure = std::stoi(opts.at("tenure"));
            if (opts.contains("opts")) {
                k_opt = std::stoi(opts.at("opts"));
                if (k_opt != 2 && k_opt != 3) {
                    throw std::invalid_argument("k_opt must be 2 or 3");
                }
            }
            if (opts.contains("neighbor")) neighborhood_size = std::stoi(opts.at("neighbor"));
            if (opts.contains("look_ahead")) look_ahead = std::stoi(opts.at("look_ahead"));
            if (opts.contains("max_worsen_ratio")) max_worsen_ratio = std::stod(opts.at("max_worsen_ratio"));
            std::random_device rd;
            rnd.seed(rd());
            app::Logger::GetInstance().AddDebug("tabu search: k_opt=" + std::to_string(k_opt) +
                                                ", neighborhood=" + std::to_string(neighborhood_size) +
                                                ", look_ahead=" + std::to_string(look_ahead) +
                                                ", max_worsen_ratio=" + std::to_string(max_worsen_ratio));
        }

        void Solve(std::vector<int> &route) override {
            auto &logger = app::Logger::GetInstance();
            const Instance &inst = Instance::GetInstance();
            const int n = inst.GetN();

            auto start = std::chrono::high_resolution_clock::now();

            logger.AddInfo("tabu search: start solve, n=" + std::to_string(inst.GetN()));

            auto nearest = tsp::SolverFactory::Create("nearest");
            auto opt_2 = tsp::SolverFactory::CreateConfigured("2-opt",
                                                              {std::make_pair("time", std::to_string(3))});
            nearest->Solve(route);
            opt_2->Solve(route);

            std::vector<int> best_route = route;
            double best_len = inst.RouteLength(route);
            double current_len = best_len;

            std::vector<std::vector<size_t>> tabu_matrix(n, std::vector<size_t>(n, 0));
            if (tenure == -1) {
                tenure = static_cast<size_t>(std::sqrt(n)) + 5;
            }

            for (size_t iter = 0; (iter < max_iter || max_iter == 0) &&
                                     (ElapsedTime(start) < time_limit || time_limit <= 0); ++iter) {

                Move best_move;

                for (size_t s = 0; s < neighborhood_size; ++s) {
                    Move candidate;
                    if (k_opt == 2) {
                        candidate = try2optLocal(route, inst);
                    } else {
                        candidate = try3optLocal(route, inst);
                    }
                    if (IsAcceptable(candidate, route, tabu_matrix, iter, current_len, best_len) &&
                        candidate.delta < best_move.delta) {
                        best_move = candidate;
                    }
                }

                if (!best_move.isValid()) break;

                const double max_worsen = std::max(1.0, current_len * max_worsen_ratio);
                if (best_move.delta > max_worsen) {
                    logger.AddDebug("tabu search: skip too bad move, delta=" + std::to_string(best_move.delta));
                    continue;
                }

                ApplyMove(route, best_move);
                current_len = inst.RouteLength(route);
                UpdateTabu(tabu_matrix, route, best_move, iter + tenure);

                if (current_len < best_len - 1e-6) {
                    best_len = current_len;
                    best_route = route;
                }

                if (iter % 1000 == 0) {
                    logger.AddInfo("curr_len=" + std::to_string(current_len) + ", best_len=" + std::to_string(best_len));
                }
            }
            route = best_route;
            logger.AddInfo("tabu search: finish solve, route_len=" + std::to_string(inst.RouteLength(route)));
        }

    private:
        struct Move {
            int i = -1, j = -1, k = -1;
            int opt_type = 2;           // = k_opt
            int case_id = 0;
            double delta = std::numeric_limits<double>::max();

            bool isValid() const { return i != -1; }
        };

        Move try2optLocal(const std::vector<int>& route, const Instance& inst) {
            int n = inst.GetN();
            if (n < 4) return {};
            std::uniform_int_distribution<int> i_dist(0, n - 3);
            int i = i_dist(rnd);
            int j_hi = std::min(n - 1, i + std::max(2, look_ahead));
            if (j_hi - i < 2) return {};
            std::uniform_int_distribution<int> j_dist(i + 2, j_hi);
            int j = j_dist(rnd);
            if (i == 0 && j == n - 1) return {};

            double delta = inst.Distance(route[i], route[j]) +
                           inst.Distance(route[i+1], route[j+1]) -
                           inst.Distance(route[i], route[i+1]) -
                           inst.Distance(route[j], route[j+1]);
            return {i, j, -1, 2, 0, delta};
        }

        Move try3optLocal(const std::vector<int>& route, const Instance& inst) {
            int n = (int)inst.GetN();
            if (n < 6) return {};
            std::uniform_int_distribution<int> i_dist(0, n - 5);
            int i = i_dist(rnd);
            int j_hi = std::min(n - 3, i + std::max(2, look_ahead));
            if (j_hi - i < 2) return {};
            std::uniform_int_distribution<int> j_dist(i + 2, j_hi);
            int j = j_dist(rnd);
            int k_hi = std::min(n - 1, j + std::max(2, look_ahead));
            if (k_hi - j < 2) return {};
            std::uniform_int_distribution<int> k_dist(j + 2, k_hi);
            int k = k_dist(rnd);
            if (i == 0 && k == n - 1) return {};

            double d_old = inst.Distance(route[i], route[i+1]) +
                           inst.Distance(route[j], route[j+1]) +
                           inst.Distance(route[k], route[k+1]);

            double d_new = inst.Distance(route[i], route[j+1]) +
                           inst.Distance(route[k], route[i+1]) +
                           inst.Distance(route[j], route[k+1]);

            return {i, j, k, 3, 4, d_new - d_old};
        }

        static bool IsAcceptable(const Move& m, const std::vector<int>& route,
                                 const std::vector<std::vector<size_t>>& tabu,
                                 size_t iter, double curr_len, double best_global) {
            if (!m.isValid()) return false;
            if (curr_len + m.delta < best_global - 1e-6) return true;

            if (m.opt_type == 2) {
                return (tabu[route[m.i]][route[m.i+1]] <= iter &&
                        tabu[route[m.j]][route[m.j+1]] <= iter);
            }
            return (tabu[route[m.i]][route[m.j+1]] <= iter &&
                    tabu[route[m.j]][route[m.k+1]] <= iter &&
                    tabu[route[m.k]][route[m.i+1]] <= iter);
        }

        static void UpdateTabu(std::vector<std::vector<size_t>>& tabu, const std::vector<int>& route, const Move& m, size_t until_iter) {
            if (m.opt_type == 2) {
                tabu[route[m.i]][route[m.i+1]] = until_iter;
                tabu[route[m.j]][route[m.j+1]] = until_iter;
            } else {
                tabu[route[m.i]][route[m.j+1]] = until_iter;
                tabu[route[m.k]][route[m.i+1]] = until_iter;
                tabu[route[m.j]][route[m.k+1]] = until_iter;
            }
        }

        void ApplyMove(std::vector<int>& route, const Move& m) {
            if (m.opt_type == 2) {
                std::reverse(route.begin() + m.i + 1, route.begin() + m.j + 1);
            } else {
                std::rotate(route.begin() + m.i + 1, route.begin() + m.j + 1, route.begin() + m.k + 1);
            }
        }

        template<typename T>
        static double ElapsedTime(T start) {
            return static_cast<double>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::high_resolution_clock::now() - start).count());
        }
    };

    static bool reg = (SolverFactory::RegisterSolver("tabu", []() {
        return std::make_unique<TabuSearch>();
    }), true);

}
