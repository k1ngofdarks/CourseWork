#include <solver.h>
#include <logger.h>
#include <factory.h>
#include <instance.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <iostream>
#include <iomanip>


namespace tsp {

    class LKHSolver : public Solver {
    private:
        double time_limit = 5.0;
        int max_trials = 1000;
        std::string lkh_path = "../LKH-2.0.11/LKH";  // Path to LKH executable
        
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
            
            const auto& latitudes = inst.GetLatitudes();
            const auto& longitudes = inst.GetLongitudes();
            
            // TSPLIB header for GEOM format (geographical coordinates in decimal degrees)
            file << "NAME: temp_problem\n";
            file << "TYPE: TSP\n";
            file << "COMMENT: Generated from geographical coordinates\n";
            file << "DIMENSION: " << n << "\n";
            file << "EDGE_WEIGHT_TYPE: GEOM\n";
            file << "NODE_COORD_SECTION\n";
            
            for (int i = 0; i < n; ++i) {
                file << (i + 1) << " " << std::fixed << std::setprecision(6)
                     << latitudes[i] << " " << longitudes[i] << "\n";
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
            file << "RUNS = 1\n";
            file << "MOVE_TYPE = 5\n";
            file << "PATCHING_C = 3\n";
            file << "PATCHING_A = 2\n";
            file << "TRACE_LEVEL = 0\n";
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
            if (opts.count("lkh_path")) {
                lkh_path = opts.at("lkh_path");
            }
        }
        
        void Solve(std::vector<int>& route) override {
            SolverLogScope log_scope(logger_, stop_token_, "lkh");
            if (log_scope.StopRequested()) {
                log_scope.Debug("stop requested before start");
                return;
            }
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
                        std::cerr << "Warning: Could not read LKH output, keeping original tour\n";
                        log_scope.Debug("could not parse output tour");
                    }
                } else {
                    std::cerr << "Warning: LKH execution failed, keeping original tour\n";
                    log_scope.Debug("lkh process failed");
                }
                
            } catch (const std::exception& e) {
                std::cerr << "LKH Solver error: " << e.what() << ", keeping original tour\n";
                log_scope.Debug(std::string("exception: ") + e.what());
            }
            
            CleanupFiles(temp_files);
            log_scope.ReportCandidate(route, CalculateRouteLength(route));
        }
    };

    static bool lkh_registered = (SolverFactory::RegisterSolver("lkh", []() {
        return std::make_unique<LKHSolver>();
    }), true);

} // namespace tsp
