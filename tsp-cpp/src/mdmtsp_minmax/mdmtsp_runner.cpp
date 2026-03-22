#include "mdmtsp_minmax/mdmtsp_runner.h"
#include "mdmtsp_minmax/mdmtsp_instance.h"
#include "mdmtsp_minmax/mdmtsp_solver_api.h"
#include <json.hpp>
#include <sstream>
#include <iostream>

namespace mdmtsp_minmax {

namespace {

std::string ReadAllStdin() {
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
}

nlohmann::json ToJson(const Solution& sol, const std::string& solver_name) {
    nlohmann::json out;
    out["status"] = "ok";
    out["solver"] = solver_name;
    out["max_route_length"] = sol.max_route_length;
    out["routes"] = nlohmann::json::array();
    for (const auto& r : sol.routes) {
        out["routes"].push_back({
            {"depot_id", r.depot_id},
            {"nodes", r.nodes},
            {"length", r.length}
        });
    }
    return out;
}

} // namespace

std::string RunMdmtspMinMax(const tsp::RunnerInput& input) {
    Instance inst = ParseInstanceFromJson(ReadAllStdin());

    std::string step = input.steps.empty() ? "greedy_seed" : input.steps.front().name;

    if (step == "random" || step == "random_template") {
        int iters = 100;
        unsigned seed = 42;
        if (!input.steps.empty()) {
            const auto& args = input.steps.front().args;
            if (args.contains("iters")) iters = std::stoi(args.at("iters"));
            if (args.contains("seed")) seed = static_cast<unsigned>(std::stoul(args.at("seed")));
        }
        return ToJson(SolveRandomTemplate(inst, iters, seed), "random_template").dump();
    }

    return ToJson(SolveGreedySeed(inst), "greedy_seed").dump();
}

} // namespace mdmtsp_minmax
