#pragma once

#include "mdmtsp_solution.h"

namespace mdmtsp_minmax {

inline double Evaluate(const Solution& s) {
    return s.max_route_length;
}

} // namespace mdmtsp_minmax
