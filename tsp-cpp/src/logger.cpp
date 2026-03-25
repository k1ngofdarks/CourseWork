#include <logger.h>
#include <instance.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace tsp {

FileStopToken::FileStopToken(std::string stop_file_path) : stop_file_path_(std::move(stop_file_path)) {}

bool FileStopToken::StopRequested() {
    if (cached_stop_state_) {
        return true;
    }
    const auto now = std::chrono::steady_clock::now();
    if (last_check_.time_since_epoch().count() == 0 ||
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_).count() >= 200) {
        cached_stop_state_ = std::filesystem::exists(stop_file_path_);
        last_check_ = now;
    }
    return cached_stop_state_;
}

FileLogger::FileLogger(Config config) : config_(std::move(config)) {
    if (!config_.csv_file.empty()) {
        std::ofstream out(config_.csv_file, std::ios::out | std::ios::trunc);
        out << "solver,event,elapsed_seconds,best_length,best_found_at_seconds\n";
    }
    if (!config_.log_file.empty()) {
        std::ofstream out(config_.log_file, std::ios::out | std::ios::trunc);
        out << "TSP logger started\n";
    }
}

void FileLogger::OnSolverStart(const std::string &solver_name) {
    WriteTextLine("[INFO] solver=" + solver_name + " event=start");
}

void FileLogger::OnImprovement(const std::string &solver_name,
                               double best_length,
                               double found_at_seconds,
                               const std::vector<int> &route) {
    (void) found_at_seconds;
    const auto now = std::chrono::steady_clock::now();
    const double global_elapsed =
        static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - session_started_at_).count()) /
        1e3;
    std::lock_guard lock(mutex_);
    auto &state = solver_states_[solver_name];
    if (!state.has_best || best_length < state.best_length) {
        state.has_best = true;
        state.best_length = best_length;
        state.best_found_at_seconds = global_elapsed;
        state.best_route = route;
    }
}

void FileLogger::OnPeriodicBest(const std::string &solver_name,
                                double best_length,
                                double elapsed_seconds,
                                const std::vector<int> &route) {
    (void) elapsed_seconds;
    const auto now = std::chrono::steady_clock::now();
    const double global_elapsed =
        static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - session_started_at_).count()) /
        1e3;
    double stored_best = best_length;
    double found_at = global_elapsed;
    {
        std::lock_guard lock(mutex_);
        auto &state = solver_states_[solver_name];
        if (!state.has_best || best_length < state.best_length) {
            state.has_best = true;
            state.best_length = best_length;
            state.best_found_at_seconds = global_elapsed;
            state.best_route = route;
        }
        stored_best = state.best_length;
        found_at = state.best_found_at_seconds;
        state.history.push_back({global_elapsed, state.best_length, state.best_found_at_seconds});
    }
    std::ostringstream ss;
    ss << "[INFO] solver=" << solver_name << " event=periodic_best best=" << std::fixed << std::setprecision(6)
       << stored_best << " elapsed=" << global_elapsed << " best_found_at=" << found_at;
    WriteTextLine(ss.str());
    WriteCsvLine(solver_name, "periodic_best", stored_best, global_elapsed, found_at);
}

void FileLogger::Debug(const std::string &solver_name, const std::string &message) {
    if (!config_.debug_enabled) {
        return;
    }
    WriteTextLine("[DEBUG] solver=" + solver_name + " " + message);
}

void FileLogger::WriteTextLine(const std::string &line) {
    if (config_.console_enabled) {
        std::lock_guard lock(mutex_);
        std::cerr << line << "\n";
    }
    if (config_.log_file.empty()) {
        return;
    }
    std::lock_guard lock(mutex_);
    std::ofstream out(config_.log_file, std::ios::out | std::ios::app);
    out << line << "\n";
}

void FileLogger::WriteCsvLine(const std::string &solver_name,
                              const std::string &event,
                              double best_length,
                              double elapsed_seconds,
                              double best_found_at_seconds) {
    if (config_.csv_file.empty()) {
        return;
    }
    std::lock_guard lock(mutex_);
    std::ofstream out(config_.csv_file, std::ios::out | std::ios::app);
    out << solver_name << "," << event << "," << std::fixed << std::setprecision(6) << elapsed_seconds << ","
        << best_length << "," << best_found_at_seconds << "\n";
}

SolverLogScope::SolverLogScope(std::shared_ptr<ILogger> logger,
                               std::shared_ptr<IStopToken> stop_token,
                               std::string solver_name,
                               double periodic_interval_seconds,
                               bool periodic_enabled,
                               bool debug_enabled)
    : logger_(std::move(logger)),
      stop_token_(std::move(stop_token)),
      solver_name_(std::move(solver_name)),
      started_at_(std::chrono::steady_clock::now()),
      last_periodic_log_at_(started_at_),
      periodic_interval_seconds_(periodic_interval_seconds),
      periodic_enabled_(periodic_enabled),
      debug_enabled_(debug_enabled) {
    if (!logger_) {
        logger_ = std::make_shared<NullLogger>();
    }
    if (!stop_token_) {
        stop_token_ = std::make_shared<NullStopToken>();
    }
    if (periodic_interval_seconds_ <= 0) {
        periodic_interval_seconds_ = logger_->PeriodicIntervalSeconds();
    }
    logger_->OnSolverStart(solver_name_);
}

SolverLogScope::~SolverLogScope() {
}

bool SolverLogScope::StopRequested() const {
    return stop_token_ && stop_token_->StopRequested();
}

void SolverLogScope::ReportCandidate(const std::vector<int> &route, double route_length) {
    if (!has_best_ || route_length < best_length_) {
        has_best_ = true;
        best_length_ = route_length;
        const auto now = std::chrono::steady_clock::now();
        const double elapsed =
            static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at_).count()) /
            1e3;
        best_route_ = route;
        logger_->OnImprovement(solver_name_, best_length_, elapsed, route);
    }
}

void SolverLogScope::TickPeriodic(const std::vector<int> &route) {
    if (!periodic_enabled_ || !has_best_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const double elapsed_since_periodic =
        static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_periodic_log_at_).count()) /
        1e3;
    if (elapsed_since_periodic >= periodic_interval_seconds_) {
        const double elapsed =
            static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at_).count()) /
            1e3;
        logger_->OnPeriodicBest(solver_name_, best_length_, elapsed, best_route_.empty() ? route : best_route_);
        last_periodic_log_at_ = now;
    }
}

void SolverLogScope::Debug(const std::string &message) const {
    if (!debug_enabled_) {
        return;
    }
    logger_->Debug(solver_name_, message);
}

std::shared_ptr<ILogger> CreateLoggerFromOptions(const std::unordered_map<std::string, std::string> &opts) {
    FileLogger::Config config;
    if (opts.contains("log_file")) {
        config.log_file = opts.at("log_file");
    }
    if (opts.contains("csv_file")) {
        config.csv_file = opts.at("csv_file");
    }
    if (opts.contains("log_interval")) {
        config.periodic_interval_seconds = std::stod(opts.at("log_interval"));
    }
    if (opts.contains("debug")) {
        config.debug_enabled = opts.at("debug") == "1" || opts.at("debug") == "true";
    }
    if (opts.contains("console_log")) {
        config.console_enabled = opts.at("console_log") == "1" || opts.at("console_log") == "true";
    }
    if (config.log_file.empty() && config.csv_file.empty() && !config.console_enabled) {
        return std::make_shared<NullLogger>();
    }
    return std::make_shared<FileLogger>(std::move(config));
}

std::shared_ptr<IStopToken> CreateStopTokenFromOptions(const std::unordered_map<std::string, std::string> &opts) {
    if (opts.contains("stop_file") && !opts.at("stop_file").empty()) {
        return std::make_shared<FileStopToken>(opts.at("stop_file"));
    }
    return std::make_shared<NullStopToken>();
}

double CalculateRouteLength(const std::vector<int> &route) {
    const Instance &inst = Instance::GetInstance();
    return inst.RouteLength(route);
}

} // namespace tsp
