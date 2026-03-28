#include <logger.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "../include/json.hpp"

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

        info_events.push_back("[INFO] Logger started for " + task_type + "/" + task_name +
                              " (mode=" + (mode == Mode::Debug ? std::string("debug") : std::string("info")) +
                              ", interval=" + std::to_string(flush_interval_sec) + "s)");

        worker = std::thread([this]() { WorkerLoop(); });
    }

    void Logger::AddInfo(const std::string &message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured) return;
        std::ostringstream out;
        out << "[INFO] t+" << std::fixed << std::setprecision(3)
            << ElapsedSecondsLocked() << "s | " << message;
        info_events.push_back(out.str());
        dirty = true;
    }

    void Logger::AddDebug(const std::string &message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured || mode != Mode::Debug) return;
        std::ostringstream out;
        out << "[DEBUG] t+" << std::fixed << std::setprecision(3)
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

        const double elapsed_seconds = ElapsedSecondsLocked();
        latest_route_snapshot = route_snapshot;

        std::ostringstream new_solution;
        new_solution << "[INFO] t+" << std::fixed << std::setprecision(3)
                     << elapsed_seconds << "s | New solution from " << source
                     << ", objective=" << objective_value;
        info_events.push_back(new_solution.str());

        const bool improved = !best_objective.has_value() || objective_value < *best_objective;
        if (improved) {
            best_objective = objective_value;
            best_route_snapshot = route_snapshot;
            std::ostringstream improvement_point;
            improvement_point << "t+" << std::fixed << std::setprecision(3) << elapsed_seconds << "s";
            improvements.push_back({elapsed_seconds, improvement_point.str(), source, objective_value, route_snapshot});
            std::ostringstream best_improved;
            best_improved << "[INFO] t+" << std::fixed << std::setprecision(3)
                          << elapsed_seconds << "s | Best improved to " << objective_value
                          << " by " << source;
            info_events.push_back(best_improved.str());
        }
        dirty = true;
    }

    void Logger::SaveNamedArtifact(const std::string &suffix, const std::string &content) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured) return;
        std::filesystem::create_directories("logs");
        std::ofstream artifact("logs/" + task_type + "_" + task_name + "_" + suffix, std::ios::out | std::ios::trunc);
        if (!artifact.is_open()) {
            return;
        }
        artifact << content;
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
        file << "===== SNAPSHOT #" << snapshot_index << " =====\n";
        file << "task=" << task_type << "/" << task_name << "\n";
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
            shutdown_message << "[INFO] t+" << std::fixed << std::setprecision(3)
                             << ElapsedSecondsLocked() << "s | Logger shutting down";
            info_events.push_back(shutdown_message.str());
            if (best_objective.has_value()) {
                std::ostringstream best_message;
                best_message << "[INFO] t+" << std::fixed << std::setprecision(3)
                             << ElapsedSecondsLocked() << "s | Final best objective=" << *best_objective;
                info_events.push_back(best_message.str());
            }
            dirty = true;
            stop_requested = true;
        }
        if (worker.joinable()) worker.join();
        std::lock_guard<std::mutex> lock(mtx);
        SaveArtifactsLocked();
        if (file.is_open()) file.close();
        configured = false;
        stop_requested = false;
        task_type.clear();
        task_name.clear();
        snapshot_index = 0;
    }

    Logger::~Logger() {
        Shutdown();
    }

    double Logger::ElapsedSecondsLocked() const {
        using namespace std::chrono;
        if (!configured) {
            return 0.0;
        }
        return duration_cast<duration<double>>(steady_clock::now() - started_monotonic).count();
    }

    void Logger::SaveArtifactsLocked() const {
        if (!configured) {
            return;
        }
        std::filesystem::create_directories("logs");
        if (!best_route_snapshot.empty()) {
            std::ofstream best_file("logs/" + task_type + "_" + task_name + "_best_route.json", std::ios::out | std::ios::trunc);
            if (best_file.is_open()) {
                best_file << best_route_snapshot;
            }
        }
        if (!latest_route_snapshot.empty()) {
            std::ofstream latest_file("logs/" + task_type + "_" + task_name + "_latest_route.json", std::ios::out | std::ios::trunc);
            if (latest_file.is_open()) {
                latest_file << latest_route_snapshot;
            }
        }
        nlohmann::json history_json;
        history_json["history"] = nlohmann::json::array();
        for (const auto &improvement: improvements) {
            history_json["history"].push_back({improvement.elapsed_seconds, improvement.objective_value});
        }
        std::ofstream history_file("logs/" + task_type + "_" + task_name + "_history.json", std::ios::out | std::ios::trunc);
        if (history_file.is_open()) {
            history_file << history_json.dump(2) << "\n";
        }
    }

} // namespace app
