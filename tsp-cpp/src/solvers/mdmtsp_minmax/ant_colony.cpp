#include <solver.h>
#include <logger.h>
#include <factory.h>

#include <algorithm>
#include <random>
#include <iostream>
#include <future>

namespace mdmtsp_minmax {

    class AntColonySolver : public Solver {
    public:
        int n_iter = 10;
        unsigned int seed = std::random_device{}();
        int n_ants = 10;
        int n_candidates = 30;
        long double alpha = 1.0;
        long double beta = 4.5;
        long double start_pheromone_level = -1.0;
        long double degradation_level = 0.9;

        void Configure(const std::unordered_map<std::string, std::string> &opts) override {
            if (opts.contains("n_iter")) n_iter = std::max(1, std::stoi(opts.at("n_iter")));
            if (opts.contains("n_ants")) n_ants = std::max(1, std::stoi(opts.at("n_ants")));
            if (opts.contains("seed")) seed = static_cast<unsigned int>(std::stoul(opts.at("seed")));
            if (opts.contains("n_candidates")) n_candidates = std::max(1, std::stoi(opts.at("n_candidates")));
            if (opts.contains("alpha")) alpha = std::stold(opts.at("alpha"));
            if (opts.contains("beta")) beta = std::stold(opts.at("beta"));
            if (opts.contains("start_pheromone_level"))
                start_pheromone_level = std::stold(opts.at("start_pheromone_level"));
            if (opts.contains("degradation_level")) degradation_level = std::stold(opts.at("degradation_level"));
        }

        void Solve(std::vector<std::vector<int>> &routes) override {
//            FILE *fp = freopen("/home/nikita/CLionProjects/CourseWork/tsp-cpp/my_logs.txt", "w", stdout);
            const auto &inst = Instance::GetInstance();
            tsp::SolverLogScope log_scope(logger_, stop_token_, "mdmtsp.ant");
            two_opt_solver->SetLogger(logger_);
            two_opt_solver->SetStopToken(stop_token_);
            nn_solver->SetLogger(logger_);
            nn_solver->SetStopToken(stop_token_);
            int n = inst.GetN();
            std::vector<int> customers = inst.GetCustomers();
            std::vector<std::vector<int>> result_routes;
            long double result_length = 1e18;

            if (start_pheromone_level <= 0) {
                std::vector<std::vector<int>> nn_routes;
                nn_solver->Solve(nn_routes);
                long double nn_result = 0.0;
                for (auto &route: nn_routes) {
                    nn_result = std::max<long double>(nn_result, inst.RouteLength(route));
                }
                start_pheromone_level = 1 / ((1 - degradation_level) * nn_result);
            }

            std::vector<std::vector<int>> candidate_lists;
            candidate_lists.reserve(n);
            for (int v = 0; v < n; ++v) {
                std::vector<int> candidate_list(customers.begin(), customers.end());
                std::nth_element(candidate_list.begin(), candidate_list.begin() + (n_candidates - 1),
                                 candidate_list.end(),
                                 [&inst, &v](int i, int j) { return inst.Distance(v, i) < inst.Distance(v, j); });
                candidate_list.resize(n_candidates);
                candidate_lists.push_back(candidate_list);
            }

            std::vector<std::vector<long double>> pheromone_level(n,
                                                                  std::vector<long double>(n, start_pheromone_level));
            std::vector<std::vector<long double>> heuristic(n, std::vector<long double>(n));

            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    if (i != j) {
                        heuristic[i][j] = 1 / std::pow(std::max<long double>(inst.Distance(i, j), 1e-5), beta);
                    }
                }
            }

            for (int epoch_id = 0; epoch_id < n_iter; ++epoch_id) {
                if (log_scope.StopRequested()) {
                    log_scope.Debug("stop requested");
                    break;
                }
                std::vector<std::vector<int>> epoch_best_routes;
                long double epoch_best_length = 1e18;

                std::vector<std::vector<long double>> intensity(n, std::vector<long double>(n));
                for (int i = 0; i < n; ++i) {
                    for (int j = 0; j < n; ++j) {
                        intensity[i][j] =
                                std::pow<long double>(std::max(pheromone_level[i][j], 1e-5L), alpha) * heuristic[i][j];
                    }
                }

                std::vector<std::future<std::pair<std::vector<std::vector<int>>, long double>>> futures;

                for (int ant_id = 0; ant_id < n_ants; ++ant_id) {
                    unsigned int ant_seed = seed + epoch_id * n_ants + ant_id;
                    futures.push_back(std::async(std::launch::async,
                                                 [this, &intensity, &candidate_lists, ant_seed]() {
                                                     std::mt19937 local_rnd(ant_seed);
                                                     return this->SimulateAntTrajectory(intensity, candidate_lists,
                                                                                        local_rnd);
                                                 }
                    ));
                }

                for (auto &f: futures) {
                    auto [curr_routes, curr_length] = f.get();
                    if (curr_length < epoch_best_length) {
                        epoch_best_length = curr_length;
                        epoch_best_routes = curr_routes;
                    }
                    if (curr_length < result_length) {
                        result_length = curr_length;
                        result_routes = curr_routes;
                        size_t max_id = 0;
                        long double max_len = 0;
                        for (size_t rid = 0; rid < curr_routes.size(); ++rid) {
                            long double len = inst.RouteLength(curr_routes[rid]);
                            if (len > max_len) {
                                max_len = len;
                                max_id = rid;
                            }
                        }
                        if (!curr_routes.empty()) {
                            log_scope.ReportCandidate(curr_routes[max_id], static_cast<double>(max_len));
                        }
                    }
                }

                long double delta = 1.0l / epoch_best_length;
                for (auto &lst: pheromone_level) {
                    for (auto &value: lst) {
                        value *= degradation_level;
                    }
                }
                for (auto &route: epoch_best_routes) {
                    for (int i = 0; i + 1 < route.size(); ++i) {
                        pheromone_level[route[i]][route[i + 1]] += delta;
                        pheromone_level[route[i + 1]][route[i]] += delta;
                    }
                }
                if (!epoch_best_routes.empty()) {
                    log_scope.TickPeriodic(epoch_best_routes.front());
                }
            }
            std::swap(routes, result_routes);
        }

    private:
        std::pair<std::vector<std::vector<int>>, long double>
        SimulateAntTrajectory(const std::vector<std::vector<long double>> &intensity,
                              const std::vector<std::vector<int>> &candidate_lists, std::mt19937 &rnd) const {
            const auto &inst = Instance::GetInstance();
            const auto &depots = inst.GetDepots();
            auto customers = inst.GetCustomers();
            int n = inst.GetN();
            int n_depots = static_cast<int>(depots.size());

            std::vector<std::vector<int>> routes(n_depots);
            std::vector<long double> route_lengths(n_depots);
            for (int i = 0; i < n_depots; ++i) {
                routes[i].push_back(depots[i]);
            }
            std::vector<bool> visited(n);
            std::uniform_real_distribution<long double> uniform_dist(0.0, 1.0);
            std::vector<long double> weights;
            std::vector<int> vis_candidates;
            weights.reserve(customers.size());
            vis_candidates.reserve(customers.size());
            int cnt_bad = 0;

            for (int customer_id = 0; customer_id < customers.size(); ++customer_id) {
                int route_id = static_cast<int>(std::min_element(route_lengths.begin(), route_lengths.end()) -
                                                route_lengths.begin());
                int curr = routes[route_id].back();
                weights.clear();
                vis_candidates.clear();

                long double sum_weights = 0;
                for (int v: candidate_lists[curr]) {
                    if (visited[v]) {
                        continue;
                    }
                    vis_candidates.push_back(v);
                    long double weight = intensity[curr][v];
                    weights.push_back(weight);
                    sum_weights += weight;
                }
                if (weights.empty()) {
                    cnt_bad++;
                    for (int v: customers) {
                        if (visited[v]) {
                            continue;
                        }
                        vis_candidates.push_back(v);
                        long double weight = intensity[curr][v];
                        weights.push_back(weight);
                        sum_weights += weight;
                    }
                }

                long double random_value = uniform_dist(rnd) * sum_weights;
                int next_vertex = *vis_candidates.begin();
                int next_vertex_id = 0;
                for (auto v: vis_candidates) {
                    random_value -= weights[next_vertex_id];
                    if (random_value <= 0) {
                        next_vertex = v;
                        break;
                    }
                    next_vertex_id++;
                }
                route_lengths[route_id] +=
                        inst.Distance(curr, next_vertex) + inst.Distance(next_vertex, depots[route_id]) -
                        inst.Distance(curr, depots[route_id]);
                routes[route_id].push_back(next_vertex);
                visited[next_vertex] = true;
            }
            for (auto &route: routes) {
                route.push_back(route[0]);
            }
            two_opt_solver->Solve(routes);
            for (int i = 0; i < n_depots; ++i) {
                route_lengths[i] = inst.RouteLength(routes[i]);
            }

            long double max_length = *std::max_element(route_lengths.begin(), route_lengths.end());
//            std::cout << cnt_bad << std::endl;
            return std::make_pair(routes, max_length);
        }

        std::unique_ptr<Solver> two_opt_solver = SolverFactory::Create("two-opt");
        std::unique_ptr<Solver> nn_solver = SolverFactory::Create("nn");
    };

    static bool reg = (SolverFactory::RegisterSolver("ant", []() {
        return std::make_unique<AntColonySolver>();
    }), true);

} // namespace mdmtsp_minmax
