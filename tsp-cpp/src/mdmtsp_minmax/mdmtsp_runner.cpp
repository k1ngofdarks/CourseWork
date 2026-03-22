#include "mdmtsp_minmax/mdmtsp_runner.h"
#include <json.hpp>

namespace mdmtsp_minmax {

std::string RunMdmtspMinMax(const tsp::RunnerInput&) {
    nlohmann::json out;
    out["status"] = "not_implemented";
    out["message"] = "mdmtsp_minmax solver pipeline scaffold is created but algorithms are not wired yet";
    out["routes"] = nlohmann::json::array();
    out["max_route_length"] = nullptr;
    return out.dump();
}

} // namespace mdmtsp_minmax
