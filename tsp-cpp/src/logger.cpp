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

        mode = new_mode;
        flush_interval_sec = std::max(1, interval_sec);
        start_time = std::chrono::steady_clock::now();
        configured = true;
        dirty = true;

        info_events.push_back(MakeEventPrefixLocked("INFO") + " Logger started for " + task_type + "/" + task_name +
                              " (interval=" + std::to_string(flush_interval_sec) + "s)");

        worker = std::thread([this]() { WorkerLoop(); });
    }

    void Logger::AddInfo(const std::string &message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured) return;
        info_events.push_back(MakeEventPrefixLocked("INFO") + " " + message);
        dirty = true;
    }

    void Logger::AddDebug(const std::string &message) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured || mode != Mode::Debug) return;
        debug_events.push_back(MakeEventPrefixLocked("DEBUG") + " " + message);
        dirty = true;
    }

    void Logger::AddNewSolution(const std::string &source, double objective_value) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!configured) return;

        info_events.push_back(MakeEventPrefixLocked("INFO") + " New solution from " + source +
                              ", objective=" + std::to_string(objective_value));

        const bool improved = !best_objective.has_value() || objective_value < *best_objective;
        if (improved) {
            best_objective = objective_value;
            improvements.push_back({NowString(), source, objective_value});
            info_events.push_back(MakeEventPrefixLocked("INFO") + " Best improved to " +
                                  std::to_string(objective_value));
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

        file << "===== SNAPSHOT " << NowString() << " =====\n";
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
        dirty = false;
    }

    void Logger::Shutdown() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (!configured) return;
            stop_requested = true;
        }
        if (worker.joinable()) worker.join();
        std::lock_guard<std::mutex> lock(mtx);
        if (file.is_open()) file.close();
        configured = false;
        stop_requested = false;
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
        return duration_cast<duration<double>>(steady_clock::now() - start_time).count();
    }

    std::string Logger::MakeEventPrefixLocked(const std::string &level) const {
        std::ostringstream out;
        out << "[" << level << "] " << NowString() << " (+"
            << std::fixed << std::setprecision(3) << ElapsedSecondsLocked() << "s) |";
        return out.str();
    }

} // namespace app
