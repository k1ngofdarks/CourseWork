#pragma once

#include <chrono>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace tsp {

class IStopToken {
public:
    virtual bool StopRequested() = 0;
    virtual ~IStopToken() = default;
};

class ILogger {
public:
    virtual void OnSolverStart(const std::string &solver_name) = 0;
    virtual void OnImprovement(const std::string &solver_name,
                               double best_length,
                               double found_at_seconds,
                               const std::vector<int> &route) = 0;
    virtual void OnPeriodicBest(const std::string &solver_name,
                                double best_length,
                                double elapsed_seconds,
                                const std::vector<int> &route) = 0;
    virtual void Debug(const std::string &solver_name, const std::string &message) = 0;
    virtual double PeriodicIntervalSeconds() const = 0;
    virtual ~ILogger() = default;
};

class NullLogger final : public ILogger {
public:
    void OnSolverStart(const std::string &) override {}
    void OnImprovement(const std::string &, double, double, const std::vector<int> &) override {}
    void OnPeriodicBest(const std::string &, double, double, const std::vector<int> &) override {}
    void Debug(const std::string &, const std::string &) override {}
    double PeriodicIntervalSeconds() const override { return 5.0; }
};

class NullStopToken final : public IStopToken {
public:
    bool StopRequested() override { return false; }
};

class FileStopToken final : public IStopToken {
public:
    explicit FileStopToken(std::string stop_file_path);
    bool StopRequested() override;

private:
    std::string stop_file_path_;
    std::chrono::steady_clock::time_point last_check_{};
    bool cached_stop_state_ = false;
};

class FileLogger final : public ILogger {
public:
    struct HistoryPoint {
        double elapsed_seconds = 0.0;
        double best_length = std::numeric_limits<double>::infinity();
        double best_found_at_seconds = 0.0;
    };

    struct SolverState {
        double best_length = std::numeric_limits<double>::infinity();
        double best_found_at_seconds = 0.0;
        std::vector<int> best_route;
        std::vector<HistoryPoint> history;
        bool has_best = false;
    };

    struct Config {
        std::string log_file;
        std::string csv_file;
        double periodic_interval_seconds = 5.0;
        bool debug_enabled = false;
        bool console_enabled = false;
    };

    explicit FileLogger(Config config);

    void OnSolverStart(const std::string &solver_name) override;
    void OnImprovement(const std::string &solver_name,
                       double best_length,
                       double found_at_seconds,
                       const std::vector<int> &route) override;
    void OnPeriodicBest(const std::string &solver_name,
                        double best_length,
                        double elapsed_seconds,
                        const std::vector<int> &route) override;
    void Debug(const std::string &solver_name, const std::string &message) override;
    double PeriodicIntervalSeconds() const override { return config_.periodic_interval_seconds; }

private:
    Config config_;
    std::mutex mutex_;
    std::unordered_map<std::string, SolverState> solver_states_;
    std::chrono::steady_clock::time_point session_started_at_{std::chrono::steady_clock::now()};

    void WriteTextLine(const std::string &line);
    void WriteCsvLine(const std::string &solver_name,
                      const std::string &event,
                      double best_length,
                      double elapsed_seconds,
                      double best_found_at_seconds);
};

class SolverLogScope {
public:
    // Creates a lightweight runtime scope for one solver execution.
    // It tracks in-memory best candidate and pushes periodic snapshots to ILogger.
    SolverLogScope(std::shared_ptr<ILogger> logger,
                   std::shared_ptr<IStopToken> stop_token,
                   std::string solver_name,
                   double periodic_interval_seconds = -1.0,
                   bool periodic_enabled = true,
                   bool debug_enabled = false);
    ~SolverLogScope();

    bool StopRequested() const;
    void ReportCandidate(const std::vector<int> &route, double route_length);
    void TickPeriodic(const std::vector<int> &route);
    void Debug(const std::string &message) const;
    template<typename... Args>
    void DebugValues(Args &&...args) const {
        std::ostringstream ss;
        (ss << ... << args);
        Debug(ss.str());
    }

private:
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IStopToken> stop_token_;
    std::string solver_name_;
    std::chrono::steady_clock::time_point started_at_;
    std::chrono::steady_clock::time_point last_periodic_log_at_;
    double best_length_ = 1e300;
    std::vector<int> best_route_;
    double periodic_interval_seconds_ = 5.0;
    bool periodic_enabled_ = true;
    bool debug_enabled_ = false;
    bool has_best_ = false;
};

std::shared_ptr<ILogger> CreateLoggerFromOptions(const std::unordered_map<std::string, std::string> &opts);
std::shared_ptr<IStopToken> CreateStopTokenFromOptions(const std::unordered_map<std::string, std::string> &opts);

double CalculateRouteLength(const std::vector<int> &route);

} // namespace tsp
