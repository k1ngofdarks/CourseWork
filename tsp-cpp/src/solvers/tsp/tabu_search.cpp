#include <solver.h>
#include <factory.h>
#include <logger.h>
#include <chrono>
#include <exception>
#include <algorithm>
#include <cmath>

namespace tsp {
    class TabuSearch : public Solver { // NOLINT
    public:
        double time_limit = -1;
        size_t max_iter = 0;
        int k_opt = 3;
        int tenure = -1;
        int look_ahead = 40; // used for restricted 3-opt neighborhood

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
            if (opts.contains("look_ahead")) look_ahead = std::stoi(opts.at("look_ahead"));
            app::Logger::GetInstance().AddDebug("tabu search: k_opt=" + std::to_string(k_opt) +
                                                ", look_ahead=" + std::to_string(look_ahead));
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
                Move best_improving;
                Move best_worsening;
                if (k_opt == 2) {
                    Enumerate2Opt(route, inst, tabu_matrix, iter, current_len, best_len, best_improving, best_worsening);
                } else {
                    Enumerate3OptRestricted(route, inst, tabu_matrix, iter, current_len, best_len, best_improving, best_worsening);
                }

                Move best_move = best_improving.isValid() ? best_improving : best_worsening;

                if (!best_move.isValid()) break;
                if (!best_improving.isValid() && best_worsening.isValid() && iter % 100 == 0) {
                    logger.AddDebug("tabu search: local optimum reached, apply least worsening delta=" + std::to_string(best_move.delta));
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

        static double Delta2Opt(const std::vector<int>& route, const Instance& inst, int i, int j) {
            return inst.Distance(route[i], route[j]) +
                   inst.Distance(route[i + 1], route[j + 1]) -
                   inst.Distance(route[i], route[i + 1]) -
                   inst.Distance(route[j], route[j + 1]);
        }

        static double Delta3OptCase4(const std::vector<int>& route, const Instance& inst, int i, int j, int k) {
            const double d_old = inst.Distance(route[i], route[i + 1]) +
                                 inst.Distance(route[j], route[j + 1]) +
                                 inst.Distance(route[k], route[k + 1]);
            const double d_new = inst.Distance(route[i], route[j + 1]) +
                                 inst.Distance(route[k], route[i + 1]) +
                                 inst.Distance(route[j], route[k + 1]);
            return d_new - d_old;
        }

        static void RegisterCandidate(const Move& candidate, Move& best_improving, Move& best_worsening) {
            if (candidate.delta < -1e-9) {
                if (candidate.delta < best_improving.delta) best_improving = candidate;
            } else if (candidate.delta < best_worsening.delta) {
                best_worsening = candidate;
            }
        }

        void Enumerate2Opt(const std::vector<int>& route, const Instance& inst,
                           const std::vector<std::vector<size_t>>& tabu, size_t iter,
                           double curr_len, double best_global,
                           Move& best_improving, Move& best_worsening) const {
            const int n = inst.GetN();
            for (int i = 0; i < n - 1; ++i) {
                for (int j = i + 2; j < n; ++j) {
                    if (i == 0 && j == n - 1) continue;
                    Move candidate{i, j, -1, 2, 0, Delta2Opt(route, inst, i, j)};
                    if (!IsAcceptable(candidate, route, tabu, iter, curr_len, best_global)) continue;
                    RegisterCandidate(candidate, best_improving, best_worsening);
                }
            }
        }

        void Enumerate3OptRestricted(const std::vector<int>& route, const Instance& inst,
                                     const std::vector<std::vector<size_t>>& tabu, size_t iter,
                                     double curr_len, double best_global,
                                     Move& best_improving, Move& best_worsening) const {
            const int n = inst.GetN();
            const int window = std::max(2, look_ahead);
            for (int i = 0; i < n - 5; ++i) {
                const int j_hi = std::min(n - 3, i + window);
                for (int j = i + 2; j <= j_hi; ++j) {
                    const int k_hi = std::min(n - 1, j + window);
                    for (int k = j + 2; k <= k_hi; ++k) {
                        if (i == 0 && k == n - 1) continue;
                        Move candidate{i, j, k, 3, 4, Delta3OptCase4(route, inst, i, j, k)};
                        if (!IsAcceptable(candidate, route, tabu, iter, curr_len, best_global)) continue;
                        RegisterCandidate(candidate, best_improving, best_worsening);
                    }
                }
            }
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
