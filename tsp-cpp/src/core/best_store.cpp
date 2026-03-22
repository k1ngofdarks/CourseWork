#include "core/best_store.h"

namespace tsp::core {

bool BestStore::TryUpdate(const std::vector<int>& route, double objective, double time_sec, std::size_t iter,
                          const std::string& algorithm) {
    if (!route_.has_value() || objective + 1e-9 < objective_) {
        route_ = route;
        objective_ = objective;
        time_sec_ = time_sec;
        iter_ = iter;
        algorithm_ = algorithm;
        return true;
    }
    return false;
}

} // namespace tsp::core
