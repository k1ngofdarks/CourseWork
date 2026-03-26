#include <factory.h>
#include <solver.h>
#include <instance.h>
#include <logger.h>

#include <chrono>
#include <iostream>
#include <numeric>
#include <string>
#include "../include/json.hpp"

struct ParsedArgs {
    std::string problem = "tsp";
    std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>> steps;
    std::unordered_map<std::string, std::string> global_opts;
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
        } else if (name == "problem") {
            parsed.problem = val;
        } else {
            args[name] = val;
            parsed.global_opts[name] = val;
        }
    }
    if (!solver_name.empty()) {
        parsed.steps.emplace_back(solver_name, args);
    }
    return parsed;
}

inline double CalculateRouteLength(const std::vector<int> &route) {
    const tsp::Instance &inst = tsp::Instance::GetInstance();
    double len = 0;
    for (size_t i = 0; i + 1 < route.size(); ++i) {
        len += inst.Distance(route[i], route[i + 1]);
    }
    return len;
}

inline double CalculateMaxRouteLength(const std::vector<std::vector<int>> &routes, std::vector<double> &lens) {
    const auto &inst = mdmtsp_minmax::Instance::GetInstance();
    lens.clear();
    lens.reserve(routes.size());
    double best = 0;
    for (const auto &route: routes) {
        double len = inst.RouteLength(route);
        lens.push_back(len);
        best = std::max(best, len);
    }
    return best;
}

inline std::string SerializeRoute(const std::vector<int> &route) {
    nlohmann::json j = route;
    return j.dump();
}

inline std::string SerializeRoutes(const std::vector<std::vector<int>> &routes) {
    nlohmann::json j = routes;
    return j.dump();
}

int main(int argc, char **argv) {
    auto parsed = ParseArguments(argc, argv);
    const std::string task_name = parsed.global_opts.contains("task_name") ? parsed.global_opts["task_name"] : "unknown_task";
    const std::string log_mode_raw = parsed.global_opts.contains("log_mode") ? parsed.global_opts["log_mode"] : "info";
    const int log_interval_sec = parsed.global_opts.contains("log_interval") ? std::max(1, std::stoi(parsed.global_opts["log_interval"])) : 5;
    const auto log_mode = (log_mode_raw == "debug") ? app::Logger::Mode::Debug : app::Logger::Mode::Info;
    auto &logger = app::Logger::GetInstance();
    logger.Configure(parsed.problem, task_name, log_mode, log_interval_sec);
    logger.AddInfo("Start solve. problem=" + parsed.problem + ", steps=" + std::to_string(parsed.steps.size()));

    if (parsed.problem == "tsp") {
        std::vector<std::unique_ptr<tsp::Solver>> solvers;
        for (const auto &[name, args]: parsed.steps) {
            solvers.push_back(std::move(tsp::SolverFactory::Create(name)));
            solvers.back()->Configure(args);
            logger.AddDebug("Configured tsp step=" + name);
        }
        const tsp::Instance &inst = tsp::Instance::GetInstance();

        std::vector<int> solution(inst.GetN() + 1);
        std::iota(solution.begin(), solution.end(), 0);
        solution.back() = 0;

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < solvers.size(); ++i) {
            auto &solver = solvers[i];
            solver->Solve(solution);
            const double candidate_len = inst.RouteLength(solution);
            logger.AddNewSolution("tsp_step_" + std::to_string(i + 1), candidate_len, SerializeRoute(solution));
        }
        auto stop = std::chrono::high_resolution_clock::now();
        double real_time =
                static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count()) /
                1e3;

        nlohmann::json j;
        j["route"] = solution;
        j["time"] = real_time;
        j["len"] = CalculateRouteLength(solution);
        logger.AddInfo("Finish tsp solve. final_len=" + std::to_string(j["len"].get<double>()));
        logger.Shutdown();
        std::cout << j.dump();
        return 0;
    }

    if (parsed.problem == "mdmtsp_minmax") {
        std::vector<std::unique_ptr<mdmtsp_minmax::Solver>> solvers;
        for (const auto &[name, args]: parsed.steps) {
            solvers.push_back(std::move(mdmtsp_minmax::SolverFactory::Create(name)));
            solvers.back()->Configure(args);
            logger.AddDebug("Configured mdmtsp step=" + name);
        }

        const auto &inst = mdmtsp_minmax::Instance::GetInstance();
        const auto &depots = inst.GetDepots();
        std::vector<std::vector<int>> solution(depots.size());
        for (size_t i = 0; i < depots.size(); ++i) {
            solution[i] = {depots[i], depots[i]};
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < solvers.size(); ++i) {
            auto &solver = solvers[i];
            solver->Solve(solution);
            std::vector<double> curr_lens;
            const double curr_max = CalculateMaxRouteLength(solution, curr_lens);
            logger.AddNewSolution("mdmtsp_step_" + std::to_string(i + 1), curr_max, SerializeRoutes(solution));
        }
        auto stop = std::chrono::high_resolution_clock::now();
        double real_time =
                static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count()) /
                1e3;

        std::vector<double> lens;
        double max_len = CalculateMaxRouteLength(solution, lens);

        nlohmann::json j;
        j["routes"] = solution;
        j["time"] = real_time;
        j["max_len"] = max_len;
        j["lens"] = lens;
        logger.AddInfo("Finish mdmtsp solve. final_max_len=" + std::to_string(max_len));
        logger.Shutdown();
        std::cout << j.dump();
        return 0;
    }

    logger.Shutdown();
    throw std::runtime_error("Unknown problem type. Supported: tsp, mdmtsp_minmax");
}
