#pragma once

#include "solver.h"
#include <stdexcept>
#include <unordered_map>

namespace tsp {
    class SolverFactory {
        inline static std::unordered_map<std::string, SolverCreator> registry;

    public:
        static void RegisterSolver(const std::string &name, SolverCreator c) {
            registry[name] = std::move(c);
        }

        static std::unique_ptr<Solver> Create(const std::string &name) {
            auto it = registry.find(name);
            if (it == registry.end()) throw std::runtime_error("No such solver in factory");
            return it->second();
        }

        static std::unique_ptr<Solver>
        CreateConfigured(const std::string &name, const std::unordered_map<std::string, std::string> &opts) {
            auto solver = Create(name);
            solver->Configure(opts);
            return solver;
        }

        static std::vector<std::string> GetList() {
            std::vector<std::string> out;
            for (auto &p: registry) out.push_back(p.first);
            return out;
        }
    };
} // namespace tsp

namespace mdmtsp_minmax {
    class SolverFactory {
        inline static std::unordered_map<std::string, SolverCreator> registry;

    public:
        static void RegisterSolver(const std::string &name, SolverCreator c) {
            registry[name] = std::move(c);
        }

        static std::unique_ptr<Solver> Create(const std::string &name) {
            auto it = registry.find(name);
            if (it == registry.end()) throw std::runtime_error("No such mdmtsp_minmax solver in factory");
            return it->second();
        }

        static std::unique_ptr<Solver>
        CreateConfigured(const std::string &name, const std::unordered_map<std::string, std::string> &opts) {
            auto solver = Create(name);
            solver->Configure(opts);
            return solver;
        }
    };
} // namespace mdmtsp_minmax
