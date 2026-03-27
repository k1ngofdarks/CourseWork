#include <solver.h>
#include <factory.h>
#include <instance.h>
#include <logger.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <cmath>
#include <iomanip>
#include <algorithm>


namespace tsp {

    class LKHSolver : public Solver {
    private:
        double time_limit = 5.0;
        int max_trials = 1000;
        int runs = 1;
        int move_type = 5;
        int patching_c = 3;
        int patching_a = 2;
        int max_candidates = 30;
        int trace_level = 0;
        bool use_geo_coordinates = false;
        std::string lkh_path = "../LKH-2.0.11/LKH";  // Path to LKH executable

        double ToTSPLIBGeoCoordinate(double decimal_degrees) const {
            const double sign = decimal_degrees < 0 ? -1.0 : 1.0;
            const double abs_value = std::abs(decimal_degrees);
            const int deg = static_cast<int>(abs_value);
            const double min_decimal = abs_value - static_cast<double>(deg);
            const double tsplib = static_cast<double>(deg) + (3.0 * min_decimal / 5.0);
            return sign * tsplib;
        }
        
        std::string GetTempFileName(const std::string& suffix) {
            auto now = std::chrono::high_resolution_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count();
            return "temp_lkh_" + std::to_string(timestamp) + suffix;
        }
        
        void CreateTSPLIBFile(const std::string& filename) {
            const Instance& inst = Instance::GetInstance();
            int n = inst.GetN();
            
            std::ofstream file(filename);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot create TSPLIB file: " + filename);
            }
            
            if (inst.HasCoordinates() && !inst.IsGeographicalMetric()) {
                const auto &x = inst.GetLatitudes();
                const auto &y = inst.GetLongitudes();
                file << "NAME: temp_problem\n";
                file << "TYPE: TSP\n";
                file << "COMMENT: Generated from euclidean coordinates\n";
                file << "DIMENSION: " << n << "\n";
                file << "EDGE_WEIGHT_TYPE: EUC_2D\n";
                file << "NODE_COORD_SECTION\n";
                for (int i = 0; i < n; ++i) {
                    file << (i + 1) << " " << std::fixed << std::setprecision(10)
                         << x[i] << " " << y[i] << "\n";
                }
                file << "EOF\n";
                file.close();
                return;
            }

            if (inst.HasCoordinates() && inst.IsGeographicalMetric() && use_geo_coordinates) {
                const auto &lat = inst.GetLatitudes();
                const auto &lon = inst.GetLongitudes();
                file << "NAME: temp_problem\n";
                file << "TYPE: TSP\n";
                file << "COMMENT: Generated from GEO coordinates\n";
                file << "DIMENSION: " << n << "\n";
                file << "EDGE_WEIGHT_TYPE: GEO\n";
                file << "NODE_COORD_SECTION\n";
                for (int i = 0; i < n; ++i) {
                    file << (i + 1) << " " << std::fixed << std::setprecision(10)
                         << ToTSPLIBGeoCoordinate(lat[i]) << " "
                         << ToTSPLIBGeoCoordinate(lon[i]) << "\n";
                }
                file << "EOF\n";
                file.close();
                return;
            }

            // Fallback for matrix input and (by default) geographical input.
            file << "NAME: temp_problem\n";
            file << "TYPE: TSP\n";
            file << "COMMENT: Generated from distance matrix\n";
            file << "DIMENSION: " << n << "\n";
            file << "EDGE_WEIGHT_TYPE: EXPLICIT\n";
            file << "EDGE_WEIGHT_FORMAT: FULL_MATRIX\n";
            file << "EDGE_WEIGHT_SECTION\n";

            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    // LKH expects integer weights in TSPLIB matrix mode.
                    const long long w = static_cast<long long>(std::llround(inst.Distance(i, j)));
                    file << w;
                    if (j + 1 < n) {
                        file << " ";
                    }
                }
                file << "\n";
            }
            
            file << "EOF\n";
            file.close();
        }
        
        // Save initial tour for LKH in TSPLIB format
        void SaveInitialTour(const std::vector<int>& route, const std::string& filename) {
            std::ofstream file(filename);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot create initial tour file: " + filename);
            }
            
            // TSPLIB tour format
            file << "NAME: initial_tour\n";
            file << "TYPE: TOUR\n";
            file << "DIMENSION: " << (route.size() - 1) << "\n";
            file << "TOUR_SECTION\n";
            
            for (size_t i = 0; i < route.size() - 1; ++i) {
                file << (route[i] + 1) << "\n";
            }
            file << "-1\n";
            file << "EOF\n";
            file.close();
        }
        
        // Create LKH parameter file
        void CreateParameterFile(const std::string& par_file, const std::string& tsp_file, 
                                const std::string& initial_tour_file, const std::string& output_file) {
            std::ofstream file(par_file);
            if (!file.is_open()) {
                throw std::runtime_error("Cannot create parameter file: " + par_file);
            }
            
            file << "PROBLEM_FILE = " << tsp_file << "\n";
            file << "INITIAL_TOUR_FILE = " << initial_tour_file << "\n";
            file << "OUTPUT_TOUR_FILE = " << output_file << "\n";
            file << "TIME_LIMIT = " << time_limit << "\n";
            file << "MAX_TRIALS = " << max_trials << "\n";
            file << "RUNS = " << runs << "\n";
            file << "MOVE_TYPE = " << move_type << "\n";
            file << "PATCHING_C = " << patching_c << "\n";
            file << "PATCHING_A = " << patching_a << "\n";
            file << "MAX_CANDIDATES = " << max_candidates << "\n";
            file << "TRACE_LEVEL = " << trace_level << "\n";
            file.close();
        }
        
        bool ReadOptimizedTour(const std::string& filename, std::vector<int>& route) {
            std::ifstream file(filename);
            if (!file.is_open()) {
                return false;
            }
            
            std::string line;
            bool in_tour_section = false;
            std::vector<int> tour_nodes;
            
            while (std::getline(file, line)) {
                if (line.find("TOUR_SECTION") != std::string::npos) {
                    in_tour_section = true;
                    continue;
                }
                
                if (in_tour_section) {
                    if (line.find("EOF") != std::string::npos || line.find("-1") != std::string::npos) {
                        break;
                    }
                    
                    std::istringstream iss(line);
                    int node;
                    while (iss >> node) {
                        if (node > 0) {
                            tour_nodes.push_back(node - 1);
                        }
                    }
                }
            }
            file.close();
            
            if (tour_nodes.empty()) {
                return false;
            }
            
            route.clear();
            route = tour_nodes;
            route.push_back(route[0]);
            
            return true;
        }
        
        // Clean up temporary files
        void CleanupFiles(const std::vector<std::string>& files) {
            for (const auto& file : files) {
                std::filesystem::remove(file);
            }
        }
        
    public:
        void Configure(const std::unordered_map<std::string, std::string>& opts) override {
            if (opts.count("time_limit")) {
                time_limit = std::stod(opts.at("time_limit"));
            }
            if (opts.count("max_trials")) {
                max_trials = std::stoi(opts.at("max_trials"));
            }
            if (opts.count("runs")) {
                runs = std::max(1, std::stoi(opts.at("runs")));
            }
            if (opts.count("move_type")) {
                move_type = std::max(2, std::stoi(opts.at("move_type")));
            }
            if (opts.count("patching_c")) {
                patching_c = std::max(0, std::stoi(opts.at("patching_c")));
            }
            if (opts.count("patching_a")) {
                patching_a = std::max(0, std::stoi(opts.at("patching_a")));
            }
            if (opts.count("max_candidates")) {
                max_candidates = std::max(2, std::stoi(opts.at("max_candidates")));
            }
            if (opts.count("trace_level")) {
                trace_level = std::max(0, std::stoi(opts.at("trace_level")));
            }
            if (opts.count("geo_mode")) {
                use_geo_coordinates = (opts.at("geo_mode") == "geo");
            }
            if (opts.count("lkh_path")) {
                lkh_path = opts.at("lkh_path");
            }
            app::Logger::GetInstance().AddDebug(
                    "lkh configured: time_limit=" + std::to_string(time_limit) +
                    ", max_trials=" + std::to_string(max_trials) +
                    ", runs=" + std::to_string(runs) +
                    ", move_type=" + std::to_string(move_type) +
                    ", patching_c=" + std::to_string(patching_c) +
                    ", patching_a=" + std::to_string(patching_a) +
                    ", max_candidates=" + std::to_string(max_candidates) +
                    ", trace_level=" + std::to_string(trace_level) +
                    ", geo_mode=" + std::string(use_geo_coordinates ? "geo" : "explicit") +
                    ", lkh_path=" + lkh_path);
        }

        void Solve(std::vector<int>& route) override {
            auto &logger = app::Logger::GetInstance();
            logger.AddInfo("lkh: start solve, n=" + std::to_string(Instance::GetInstance().GetN()));
            std::string tsp_file = GetTempFileName(".tsp");
            std::string par_file = GetTempFileName(".par");
            std::string initial_tour_file = GetTempFileName(".tour");
            std::string output_file = GetTempFileName(".out");
            
            std::vector<std::string> temp_files = {tsp_file, par_file, initial_tour_file, output_file};
            
            try {
                CreateTSPLIBFile(tsp_file);
                
                SaveInitialTour(route, initial_tour_file);
                
                CreateParameterFile(par_file, tsp_file, initial_tour_file, output_file);
                
                std::string command = lkh_path + " " + par_file;
#ifdef _WIN32
                command += " > NUL 2>&1";       /// TODO: Check | MAYBE NOT WORK :)
#else
                command += " > /dev/null 2>&1";
#endif
                
                int result = std::system(command.c_str());
                
                if (result == 0) {
                    if (!ReadOptimizedTour(output_file, route)) {
                        logger.AddInfo("lkh: output read failed, keeping original route");
                        std::cerr << "Warning: Could not read LKH output, keeping original tour\n";
                    }
                } else {
                    logger.AddInfo("lkh: external process failed, keeping original route");
                    std::cerr << "Warning: LKH execution failed, keeping original tour\n";
                }
                
            } catch (const std::exception& e) {
                logger.AddInfo("lkh: exception, keeping original route");
                std::cerr << "LKH Solver error: " << e.what() << ", keeping original tour\n";
            }
            
            CleanupFiles(temp_files);
            logger.AddInfo("lkh: finish solve, route_len=" + std::to_string(Instance::GetInstance().RouteLength(route)));
        }
    };

    static bool lkh_registered = (SolverFactory::RegisterSolver("lkh", []() {
        return std::make_unique<LKHSolver>();
    }), true);

} // namespace tsp
