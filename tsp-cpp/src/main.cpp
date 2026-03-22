#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "tsp/stop_condition.h"
#include "tsp/tsp_runner.h"
#include "mdmtsp_minmax/mdmtsp_runner.h"

struct ParsedArgs {
    std::string problem_type = "tsp";
    tsp::RunnerInput runner_input;
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
                parsed.runner_input.steps.push_back({solver_name, args});
                args.clear();
            }
            solver_name = val;
            continue;
        }

        if (name == "problem") {
            parsed.problem_type = val;
            continue;
        }

        if (name.rfind("run_", 0) == 0 || name.rfind("log_", 0) == 0 || name.rfind("checkpoint_", 0) == 0) {
            parsed.runner_input.global_opts[name] = val;
        } else {
            args[name] = val;
        }
    }

    if (!solver_name.empty()) {
        parsed.runner_input.steps.push_back({solver_name, args});
    }

    return parsed;
}

int main(int argc, char **argv) {
    tsp::StopCondition::InstallSignalHandlers();
    auto parsed = ParseArguments(argc, argv);

    if (parsed.problem_type == "mdmtsp_minmax") {
        std::cout << mdmtsp_minmax::RunMdmtspMinMax(parsed.runner_input);
        return 0;
    }

    std::cout << tsp::RunTsp(parsed.runner_input);
    return 0;
}
