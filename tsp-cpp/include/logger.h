#pragma once

#include <chrono>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace app {

    class Logger {
    public:
        enum class Mode {
            Info,
            Debug,
        };

        static Logger &GetInstance();

        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;
        Logger(Logger &&) = delete;
        Logger &operator=(Logger &&) = delete;

        void Configure(const std::string &task_type,
                       const std::string &task_name,
                       Mode mode,
                       int flush_interval_sec);

        void AddInfo(const std::string &message);
        void AddDebug(const std::string &message);
        void AddNewSolution(const std::string &source, double objective_value);
        void AddNewSolution(const std::string &source, double objective_value, const std::string &route_snapshot);

        void Shutdown();

    private:
        Logger() = default;
        ~Logger();

        struct ImprovementPoint {
            std::string timestamp;
            std::string source;
            double objective_value;
            std::string route_snapshot;
        };

        void WorkerLoop();
        void FlushSnapshotLocked();
        static std::string NowString();
        double ElapsedSecondsLocked() const;

        std::mutex mtx;
        std::ofstream file;
        bool configured = false;
        bool stop_requested = false;
        bool dirty = false;
        Mode mode = Mode::Info;
        int flush_interval_sec = 5;
        std::thread worker;

        std::vector<std::string> info_events;
        std::vector<std::string> debug_events;
        std::vector<ImprovementPoint> improvements;
        std::optional<double> best_objective;
        std::string latest_route_snapshot;
        std::string best_route_snapshot;
        std::string task_type;
        std::string task_name;
        std::string started_at;
        std::chrono::steady_clock::time_point started_monotonic;
        size_t snapshot_index = 0;
    };

} // namespace app
