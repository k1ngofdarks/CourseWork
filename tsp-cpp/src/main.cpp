#include <factory.h>
#include <solver.h>
#include <iostream>
#include <numeric>
#include <chrono>
#include <fstream>
#include <csignal>
#include <cmath>
#include "../include/json.hpp"

struct ParsedArgs {
    std::unordered_map<std::string, std::string> global_opts;
    std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>> steps;
};

inline ParsedArgs ParseArguments(int argc, char **argv) {
    ParsedArgs parsed;
    std::unordered_map<std::string, std::string> args;
    std::string solver_name;
    for (int i = 1; i < argc - 1; i += 2) {
        std::string name = argv[i];
        std::string val = argv[i + 1];
        if (name.size() < 3 || name[0] != '-' || name[1] != '-') {
            throw std::runtime_error("Unexpected argument");
        }
        name = name.substr(2);
        if (name == "step") {
            if (!solver_name.empty()) {
                parsed.steps.emplace_back(solver_name, args);
                args.clear();
            }
            solver_name = val;
        } else if (name.rfind("run_", 0) == 0 || name.rfind("log_", 0) == 0 || name.rfind("checkpoint_", 0) == 0) {
            parsed.global_opts[name] = val;
        } else {
            args[name] = val;
        }
    }
    if (!solver_name.empty()) {
        parsed.steps.emplace_back(solver_name, args);
    }
    return parsed;
}

inline double CalculateRouteLength(const std::vector<int> &route){
    const tsp::Instance &inst = tsp::Instance::GetInstance();
    return inst.RouteLength(route);
}

volatile std::sig_atomic_t stop_signal = 0;

void SignalHandler(int) {
    stop_signal = 1;
}

class RunLogger {
public:
    explicit RunLogger(std::string history_path, std::string checkpoint_path, double save_every_sec)
            : history_path_(std::move(history_path)), checkpoint_path_(std::move(checkpoint_path)),
              save_every_sec_(save_every_sec) {}

    void TryUpdate(const std::vector<int> &route,
                   std::size_t iter,
                   const std::string &algorithm,
                   const std::chrono::high_resolution_clock::time_point &run_start,
                   bool force_checkpoint = false) {
        double length = CalculateRouteLength(route);
        double t = Elapsed(run_start);
        if (!best_route_.has_value() || length + 1e-9 < best_length_) {
            best_route_ = route;
            best_length_ = length;
            best_iter_ = iter;
            best_solver_ = algorithm;
            best_time_sec_ = t;
            WriteHistoryEvent(t, iter, algorithm, length, route);
            WriteCheckpoint();
            last_checkpoint_flush_sec_ = t;
            return;
        }

        if ((force_checkpoint || (save_every_sec_ > 0 && t - last_checkpoint_flush_sec_ >= save_every_sec_)) && best_route_.has_value()) {
            WriteCheckpoint();
            last_checkpoint_flush_sec_ = t;
        }
    }

    [[nodiscard]] bool HasBest() const { return best_route_.has_value(); }
    [[nodiscard]] const std::vector<int> &BestRoute() const { return best_route_.value(); }
    [[nodiscard]] double BestLength() const { return best_length_; }
    [[nodiscard]] double BestTime() const { return best_time_sec_; }
    [[nodiscard]] std::size_t BestIter() const { return best_iter_; }
    [[nodiscard]] const std::string &BestSolver() const { return best_solver_; }

private:
    static double Elapsed(const std::chrono::high_resolution_clock::time_point &start) {
        return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count()) / 1e3;
    }

    void WriteHistoryEvent(double t,
                           std::size_t iter,
                           const std::string &algorithm,
                           double length,
                           const std::vector<int> &route) const {
        if (history_path_.empty()) return;
        nlohmann::json e;
        e["time_sec"] = t;
        e["iter"] = iter;
        e["algorithm"] = algorithm;
        e["length"] = length;
        e["route"] = route;
        std::ofstream out(history_path_, std::ios::app);
        out << e.dump() << "\n";
    }

    void WriteCheckpoint() const {
        if (checkpoint_path_.empty() || !best_route_.has_value()) return;
        nlohmann::json c;
        c["best_route"] = best_route_.value();
        c["best_length"] = best_length_;
        c["best_time_sec"] = best_time_sec_;
        c["best_iter"] = best_iter_;
        c["algorithm"] = best_solver_;
        std::string tmp = checkpoint_path_ + ".tmp";
        {
            std::ofstream out(tmp);
            out << c.dump(2);
        }
        std::rename(tmp.c_str(), checkpoint_path_.c_str());
    }

    std::string history_path_;
    std::string checkpoint_path_;
    double save_every_sec_ = 30.0;
    double last_checkpoint_flush_sec_ = 0.0;

    std::optional<std::vector<int>> best_route_;
    double best_length_ = std::numeric_limits<double>::infinity();
    double best_time_sec_ = 0.0;
    std::size_t best_iter_ = 0;
    std::string best_solver_;
};

int main(int argc, char **argv) {
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    auto parsed = ParseArguments(argc, argv);

    std::vector<std::pair<std::string, std::unique_ptr<tsp::Solver>>> solvers;
    for (const auto &[name, args]: parsed.steps) {
        auto solver = tsp::SolverFactory::Create(name);
        solver->Configure(args);
        solvers.emplace_back(name, std::move(solver));
    }
    const tsp::Instance &inst = tsp::Instance::GetInstance();

    std::vector<int> solution(inst.GetN() + 1);
    std::iota(solution.begin(), solution.end(), 0);
    solution.back() = 0;

    double run_time_limit = -1;
    if (parsed.global_opts.contains("run_time_limit")) {
        run_time_limit = std::stod(parsed.global_opts.at("run_time_limit"));
    }
    double save_every = 30.0;
    if (parsed.global_opts.contains("checkpoint_every_sec")) {
        save_every = std::stod(parsed.global_opts.at("checkpoint_every_sec"));
    }
    std::string history_file = parsed.global_opts.contains("log_history_file")
                               ? parsed.global_opts.at("log_history_file")
                               : "";
    std::string checkpoint_file = parsed.global_opts.contains("checkpoint_file")
                                  ? parsed.global_opts.at("checkpoint_file")
                                  : "";

    RunLogger logger(history_file, checkpoint_file, save_every);

    auto start = std::chrono::high_resolution_clock::now();
    auto should_stop = [&start, run_time_limit]() {
        if (stop_signal) return true;
        if (run_time_limit <= 0) return false;
        double elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count()) / 1e3;
        return elapsed >= run_time_limit;
    };

    std::size_t global_iter = 0;
    for (size_t i = 0; i < solvers.size(); ++i) {
        auto &solver_name = solvers[i].first;
        auto &solver = solvers[i].second;
        solver->SetCallbacks({
                should_stop,
                [&](const std::vector<int>& route, std::size_t iter) {
                    global_iter = std::max(global_iter, iter);
                    logger.TryUpdate(route, iter, solver_name, start);
                }
        });
        solver->Solve(solution);
        logger.TryUpdate(solution, global_iter, solver_name, start, true);
        if (should_stop()) break;
    }

    auto stop = std::chrono::high_resolution_clock::now();
    double real_time =
            static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count()) / 1e3;

    if (logger.HasBest()) {
        solution = logger.BestRoute();
    }

    nlohmann::json j;
    j["route"] = solution;
    j["time"] = real_time;
    j["len"] = CalculateRouteLength(solution);
    j["best_found_time"] = logger.HasBest() ? logger.BestTime() : real_time;
    j["best_iter"] = logger.HasBest() ? logger.BestIter() : 0;
    j["best_solver"] = logger.HasBest() ? logger.BestSolver() : "";
    j["stopped_by_signal"] = (stop_signal != 0);
    std::cout << j.dump();
    return 0;
}
