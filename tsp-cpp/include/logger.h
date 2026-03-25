#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
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
    virtual ~ILogger() = default;
};

class NullLogger final : public ILogger {
public:
    void OnSolverStart(const std::string &) override {}
    void OnImprovement(const std::string &, double, double, const std::vector<int> &) override {}
    void OnPeriodicBest(const std::string &, double, double, const std::vector<int> &) override {}
    void Debug(const std::string &, const std::string &) override {}
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

private:
    Config config_;
    std::mutex mutex_;

    std::string SerializeRoute(const std::vector<int> &route) const;
    void WriteTextLine(const std::string &line);
    void WriteCsvLine(const std::string &solver_name,
                      const std::string &event,
                      double best_length,
                      double seconds,
                      const std::vector<int> &route);
};

class SolverLogScope {
public:
    SolverLogScope(std::shared_ptr<ILogger> logger,
                   std::shared_ptr<IStopToken> stop_token,
                   std::string solver_name,
                   double periodic_interval_seconds = 5.0);

    bool StopRequested() const;
    void ReportCandidate(const std::vector<int> &route, double route_length);
    void TickPeriodic(const std::vector<int> &route);
    void Debug(const std::string &message) const;

private:
    std::shared_ptr<ILogger> logger_;
    std::shared_ptr<IStopToken> stop_token_;
    std::string solver_name_;
    std::chrono::steady_clock::time_point started_at_;
    std::chrono::steady_clock::time_point last_periodic_log_at_;
    double best_length_ = 1e300;
    double periodic_interval_seconds_ = 5.0;
    bool has_best_ = false;
};

std::shared_ptr<ILogger> CreateLoggerFromOptions(const std::unordered_map<std::string, std::string> &opts);
std::shared_ptr<IStopToken> CreateStopTokenFromOptions(const std::unordered_map<std::string, std::string> &opts);

double CalculateRouteLength(const std::vector<int> &route);

} // namespace tsp
