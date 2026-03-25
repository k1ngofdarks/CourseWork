#include <logger.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace app {

    Logger &Logger::GetInstance() {
        static Logger inst;
        return inst;
    }

    void Logger::Configure(const std::string &task_type,
                           const std::string &task_name,
                           Mode new_mode,
                           int interval_sec) {
        std::lock_guard<std::mutex> lock(mtx);
        if (configured) return;

        std::filesystem::create_directories("logs");
        const std::string path = "logs/" + task_type + "_" + task_name + ".log";
        file.open(path, std::ios::out | std::ios::trunc);

        this->task_type = task_type;
        this->task_name = task_name;
        started_at = NowString();
        started_monotonic = std::chrono::steady_clock::now();
        snapshot_index = 0;
        mode = new_mode;
        flush_interval_sec = std::max(1, interval_sec);
        info_events.clear();
        debug_events.clear();
        improvements.clear();
        best_objective.reset();
        latest_route_snapshot.clear();
        best_route_snapshot.clear();
        configured = true;
        dirty = true;

        info_events.push_back("[INFO] " + started_at + " | Logger started for " + task_type + "/" + task_name +
                              " (mode=" + (mode == Mode::Debug ? std::string("debug") : std::string("info")) +
                              ", interval=" + std::to_string(flush_interval_sec) + "s)");

        worker = std::thread([this]() { WorkerLoop(); });
    }

    void Logger::AddInfo(const std::string &message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured) return;
        std::ostringstream out;
        out << "[INFO] " << NowString() << " | t+" << std::fixed << std::setprecision(3)
            << ElapsedSecondsLocked() << "s | " << message;
        info_events.push_back(out.str());
        dirty = true;
    }

    void Logger::AddDebug(const std::string &message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured || mode != Mode::Debug) return;
        std::ostringstream out;
        out << "[DEBUG] " << NowString() << " | t+" << std::fixed << std::setprecision(3)
            << ElapsedSecondsLocked() << "s | " << message;
        debug_events.push_back(out.str());
        dirty = true;
    }

    void Logger::AddNewSolution(const std::string &source, double objective_value) {
        AddNewSolution(source, objective_value, "");
    }

    void Logger::AddNewSolution(const std::string &source, double objective_value, const std::string &route_snapshot) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured) return;

        latest_route_snapshot = route_snapshot;

        std::ostringstream new_solution;
        new_solution << "[INFO] " << NowString() << " | t+" << std::fixed << std::setprecision(3)
                     << ElapsedSecondsLocked() << "s | New solution from " << source
                     << ", objective=" << objective_value;
        info_events.push_back(new_solution.str());

        const bool improved = !best_objective.has_value() || objective_value < *best_objective;
        if (improved) {
            best_objective = objective_value;
            best_route_snapshot = route_snapshot;
            improvements.push_back({NowString(), source, objective_value, route_snapshot});
            std::ostringstream best_improved;
            best_improved << "[INFO] " << NowString() << " | t+" << std::fixed << std::setprecision(3)
                          << ElapsedSecondsLocked() << "s | Best improved to " << objective_value
                          << " by " << source;
            info_events.push_back(best_improved.str());
        }
        dirty = true;
    }

    void Logger::WorkerLoop() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(flush_interval_sec));
            std::lock_guard<std::mutex> lock(mtx);
            if (stop_requested) {
                FlushSnapshotLocked();
                return;
            }
            FlushSnapshotLocked();
        }
    }

    void Logger::FlushSnapshotLocked() {
        if (!configured || !file.is_open() || !dirty) return;

        ++snapshot_index;
        file << "===== SNAPSHOT #" << snapshot_index << " " << NowString() << " =====\n";
        file << "task=" << task_type << "/" << task_name << "\n";
        file << "started_at=" << started_at << "\n";
        file << "elapsed_seconds=" << std::fixed << std::setprecision(3) << ElapsedSecondsLocked() << "\n";
        if (best_objective.has_value()) {
            file << "best_objective=" << *best_objective << "\n";
        } else {
            file << "best_objective=none\n";
        }
        file << "improvements_count=" << improvements.size() << "\n";
        for (const auto &x: improvements) {
            file << "  * " << x.timestamp << " | " << x.source << " | " << x.objective_value << "\n";
        }

        file << "events:\n";
        for (const auto &e: info_events) file << "  " << e << "\n";
        if (mode == Mode::Debug) {
            for (const auto &e: debug_events) file << "  " << e << "\n";
        }
        file << "\n";
        file.flush();
        info_events.clear();
        debug_events.clear();
        dirty = false;
    }

    void Logger::Shutdown() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (!configured) return;
            std::ostringstream shutdown_message;
            shutdown_message << "[INFO] " << NowString() << " | t+" << std::fixed << std::setprecision(3)
                             << ElapsedSecondsLocked() << "s | Logger shutting down";
            info_events.push_back(shutdown_message.str());
            if (best_objective.has_value()) {
                std::ostringstream best_message;
                best_message << "[INFO] " << NowString() << " | t+" << std::fixed << std::setprecision(3)
                             << ElapsedSecondsLocked() << "s | Final best objective=" << *best_objective;
                info_events.push_back(best_message.str());
            }
            dirty = true;
            stop_requested = true;
        }
        if (worker.joinable()) worker.join();
        std::lock_guard<std::mutex> lock(mtx);
        if (file.is_open()) file.close();
        configured = false;
        stop_requested = false;
        task_type.clear();
        task_name.clear();
        started_at.clear();
        snapshot_index = 0;
    }

    Logger::~Logger() {
        Shutdown();
    }

    std::string Logger::NowString() {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const std::time_t t = system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&t);
        std::ostringstream out;
        out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return out.str();
    }

    double Logger::ElapsedSecondsLocked() const {
        using namespace std::chrono;
        if (!configured) {
            return 0.0;
        }
        return duration_cast<duration<double>>(steady_clock::now() - started_monotonic).count();
    }

} // namespace app
