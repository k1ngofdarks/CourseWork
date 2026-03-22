#include "tsp/tsp_runner.h"

#include "tsp/factory.h"
#include "tsp/instance.h"
#include <numeric>
#include <fstream>
#include <optional>
#include <chrono>
#include <cstdio>
#include <json.hpp>
#include "tsp/best_store.h"
#include "tsp/stop_condition.h"

namespace tsp {
namespace {

inline double CalculateRouteLength(const std::vector<int>& route) {
    return Instance::GetInstance().RouteLength(route);
}

class RunLogger {
public:
    explicit RunLogger(std::string history_path, std::string checkpoint_path, double save_every_sec)
        : history_path_(std::move(history_path)), checkpoint_path_(std::move(checkpoint_path)),
          save_every_sec_(save_every_sec) {}

    void TryUpdate(const std::vector<int>& route,
                   std::size_t iter,
                   const std::string& algorithm,
                   const std::chrono::high_resolution_clock::time_point& run_start,
                   bool force_checkpoint = false) {
        double length = CalculateRouteLength(route);
        double t = Elapsed(run_start);
        if (best_.TryUpdate(route, length, t, iter, algorithm)) {
            WriteHistoryEvent(t, iter, algorithm, length, route);
            WriteCheckpoint();
            last_checkpoint_flush_sec_ = t;
            return;
        }

        if ((force_checkpoint || (save_every_sec_ > 0 && t - last_checkpoint_flush_sec_ >= save_every_sec_)) && best_.HasValue()) {
            WriteCheckpoint();
            last_checkpoint_flush_sec_ = t;
        }
    }

    [[nodiscard]] const BestStore& Best() const { return best_; }

private:
    static double Elapsed(const std::chrono::high_resolution_clock::time_point& start) {
        return static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start).count()) / 1e3;
    }

    void WriteHistoryEvent(double t,
                           std::size_t iter,
                           const std::string& algorithm,
                           double length,
                           const std::vector<int>& route) const {
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
        if (checkpoint_path_.empty() || !best_.HasValue()) return;
        nlohmann::json c;
        c["best_route"] = best_.Route();
        c["best_length"] = best_.Objective();
        c["best_time_sec"] = best_.TimeSec();
        c["best_iter"] = best_.Iter();
        c["algorithm"] = best_.Algorithm();
        std::string tmp = checkpoint_path_ + ".tmp";
        { std::ofstream out(tmp); out << c.dump(2); }
        std::rename(tmp.c_str(), checkpoint_path_.c_str());
    }

    std::string history_path_;
    std::string checkpoint_path_;
    double save_every_sec_ = 30.0;
    double last_checkpoint_flush_sec_ = 0.0;
    BestStore best_;
};

} // namespace

std::string RunTsp(const RunnerInput& input) {
    std::vector<std::pair<std::string, std::unique_ptr<Solver>>> solvers;
    for (const auto& step : input.steps) {
        auto solver = SolverFactory::Create(step.name);
        solver->Configure(step.args);
        solvers.emplace_back(step.name, std::move(solver));
    }

    const Instance& inst = Instance::GetInstance();
    std::vector<int> solution(inst.GetN() + 1);
    std::iota(solution.begin(), solution.end(), 0);
    solution.back() = 0;

    double run_time_limit = -1;
    if (input.global_opts.contains("run_time_limit")) run_time_limit = std::stod(input.global_opts.at("run_time_limit"));
    double checkpoint_every = 30.0;
    if (input.global_opts.contains("checkpoint_every_sec")) checkpoint_every = std::stod(input.global_opts.at("checkpoint_every_sec"));

    std::string history_file = input.global_opts.contains("log_history_file") ? input.global_opts.at("log_history_file") : "";
    std::string checkpoint_file = input.global_opts.contains("checkpoint_file") ? input.global_opts.at("checkpoint_file") : "";

    RunLogger logger(history_file, checkpoint_file, checkpoint_every);
    auto start = std::chrono::high_resolution_clock::now();

    StopCondition stop(run_time_limit);

    std::size_t global_iter = 0;
    for (auto& [solver_name, solver] : solvers) {
        solver->SetCallbacks({
            [&]() { return stop.ShouldStop(); },
            [&](const std::vector<int>& route, std::size_t iter) {
                global_iter = std::max(global_iter, iter);
                logger.TryUpdate(route, iter, solver_name, start);
            }
        });

        solver->Solve(solution);
        logger.TryUpdate(solution, global_iter, solver_name, start, true);
        if (stop.ShouldStop()) break;
    }

    auto stop_time = std::chrono::high_resolution_clock::now();
    double real_time = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start).count()) / 1e3;

    if (logger.Best().HasValue()) solution = logger.Best().Route();

    nlohmann::json out;
    out["route"] = solution;
    out["time"] = real_time;
    out["len"] = CalculateRouteLength(solution);
    out["best_found_time"] = logger.Best().HasValue() ? logger.Best().TimeSec() : real_time;
    out["best_iter"] = logger.Best().HasValue() ? logger.Best().Iter() : 0;
    out["best_solver"] = logger.Best().HasValue() ? logger.Best().Algorithm() : "";
    out["stopped_by_signal"] = StopCondition::IsSignalStopRequested();
    return out.dump();
}

} // namespace tsp
