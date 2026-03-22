#pragma once

#include "mdmtsp_instance.h"
#include "mdmtsp_solution.h"

namespace mdmtsp_minmax {

Solution SolveGreedySeed(const Instance& inst);
Solution SolveRandomTemplate(const Instance& inst, int iterations, unsigned seed);

} // namespace mdmtsp_minmax
