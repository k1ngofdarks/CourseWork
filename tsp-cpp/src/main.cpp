#include <factory.h>
#include <solver.h>
#include <iostream>
#include <numeric>
#include <chrono>
#include "../include/json.hpp"

inline std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>>
ParseArguments(int argc, char **argv) {
    std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>> steps;
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
                steps.emplace_back(solver_name, args);
                args.clear();
            }
            solver_name = val;
        } else {
            args[name] = val;
        }
    }
    if (!solver_name.empty()) {
        steps.emplace_back(solver_name, args);
    }
    return steps;
}

inline double CalculateRouteLength(const std::vector<int> &route){
    const tsp::Instance &inst = tsp::Instance::GetInstance();
    double len = 0;
    for (size_t i = 0; i + 1 < route.size(); ++i){
        len += inst.Distance(route[i], route[i + 1]);
    }
    return len;
}

int main(int argc, char **argv) {
    auto steps = ParseArguments(argc, argv);

    std::vector<std::unique_ptr<tsp::Solver>> solvers;
    for (const auto &[name, args]: steps) {
        solvers.push_back(std::move(tsp::SolverFactory::Create(name)));
        solvers.back()->Configure(args);
    }
    const tsp::Instance &inst = tsp::Instance::GetInstance();

    std::vector<int> solution(inst.GetN() + 1);
    std::iota(solution.begin(), solution.end(), 0);
    solution.back() = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < solvers.size(); ++i) {
        solvers[i]->Solve(solution);
    }
    auto stop = std::chrono::high_resolution_clock::now();
    double real_time =
            static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count()) / 1e3;
    nlohmann::json j;
    j["route"] = solution;
    j["time"] = real_time;
    j["len"] = CalculateRouteLength(solution);
    std::cout << j.dump();
    return 0;
}