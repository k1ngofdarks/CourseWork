#include <factory.h>
#include <solver.h>
#include <instance.h>

#include <chrono>
#include <iostream>
#include <numeric>
#include <string>
#include "../include/json.hpp"

struct ParsedArgs {
    std::string problem = "tsp";
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
        } else if (name == "problem") {
            parsed.problem = val;
        } else {
            args[name] = val;
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

int main(int argc, char **argv) {
    auto parsed = ParseArguments(argc, argv);

    if (parsed.problem == "tsp") {
        std::vector<std::unique_ptr<tsp::Solver>> solvers;
        for (const auto &[name, args]: parsed.steps) {
            solvers.push_back(std::move(tsp::SolverFactory::Create(name)));
            solvers.back()->Configure(args);
        }
        const tsp::Instance &inst = tsp::Instance::GetInstance();

        std::vector<int> solution(inst.GetN() + 1);
        std::iota(solution.begin(), solution.end(), 0);
        solution.back() = 0;

        auto start = std::chrono::high_resolution_clock::now();
        for (auto &solver: solvers) {
            solver->Solve(solution);
        }
        auto stop = std::chrono::high_resolution_clock::now();
        double real_time =
                static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count()) /
                1e3;

        nlohmann::json j;
        j["route"] = solution;
        j["time"] = real_time;
        j["len"] = CalculateRouteLength(solution);
        std::cout << j.dump();
        return 0;
    }

    if (parsed.problem == "mdmtsp_minmax") {
        std::vector<std::unique_ptr<mdmtsp_minmax::Solver>> solvers;
        for (const auto &[name, args]: parsed.steps) {
            solvers.push_back(std::move(mdmtsp_minmax::SolverFactory::Create(name)));
            solvers.back()->Configure(args);
        }

        const auto &inst = mdmtsp_minmax::Instance::GetInstance();
        const auto &depots = inst.GetDepots();
        std::vector<std::vector<int>> solution(depots.size());
        for (size_t i = 0; i < depots.size(); ++i) {
            solution[i] = {depots[i], depots[i]};
        }

        auto start = std::chrono::high_resolution_clock::now();
        for (auto &solver: solvers) {
            solver->Solve(solution);
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
        std::cout << j.dump();
        return 0;
    }

    throw std::runtime_error("Unknown problem type. Supported: tsp, mdmtsp_minmax");
}
