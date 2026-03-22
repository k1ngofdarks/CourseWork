#pragma once

#include "tsp_solution.h"
#include "tsp_instance.h"

namespace tsp {

inline double EvaluateTSP(const TSPSolution& solution) {
    return Instance::GetInstance().RouteLength(solution.route);
}

} // namespace tsp
