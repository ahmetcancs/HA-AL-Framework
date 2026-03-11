#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <string>
#include <sstream>
#include <omp.h>
#include <papi.h>
#include <pthread.h>
#include <algorithm>
#include <iomanip>

unsigned long get_thread_id() {
    return (unsigned long)pthread_self(); 
}

class Graph {
public:
    int num_nodes = 0;
    std::vector<std::vector<int>> adj;
    long long* degrees; 

    void load(std::string filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open dataset file!" << std::endl;
            exit(1);
        }
        std::string line;
        int u, v, max_n = -1;
        std::vector<std::pair<int, int>> edges;
        
        while (std::getline(file, line)) {
            if (line[0] == '#') continue; 
            std::stringstream ss(line);
            if (ss >> u >> v) {
                edges.push_back({u, v});
                if (u > max_n) max_n = u;
                if (v > max_n) max_n = v;
            }
        }
        
        num_nodes = max_n + 1;
        adj.resize(num_nodes);
        degrees = new long long[num_nodes];

        #pragma omp parallel for schedule(static)
        for (int i = 0; i < num_nodes; i++) {
            degrees[i] = 0; 
        }
        
        for (auto& e : edges) {
            adj[e.first].push_back(e.second);
            adj[e.second].push_back(e.first);
            degrees[e.first]++; 
            degrees[e.second]++;
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: ./ha_al_executable <dataset.txt>" << std::endl;
        return 1;
    }

    PAPI_library_init(PAPI_VER_CURRENT);
    PAPI_thread_init(get_thread_id);

    Graph g;
    g.load(argv[1]);

    int* community = new int[g.num_nodes];
    long long* comm_weight = new long long[g.num_nodes];
    long long m2 = 0; 
    
    omp_lock_t* locks = new omp_lock_t[g.num_nodes];

    #pragma omp parallel for reduction(+:m2) schedule(static)
    for(int i = 0; i < g.num_nodes; i++) {
        community[i] = i;              
        comm_weight[i] = g.degrees[i]; 
        omp_init_lock(&locks[i]); 
        m2 += g.degrees[i];
    }

    // HA-AL Phase 1: Dynamic Density Threshold
    const long long DENSITY_THRESHOLD = 10000;
    
    // HA-AL Phase 2: Hardware-Agnostic PAPI Threshold (Approx. 5% of L3 Capacity)
    const long long L3_MISS_DANGER_ZONE = 10000000; 

    int current_threads = omp_get_max_threads(); // Start with maximum hardware threads
    omp_set_dynamic(0); // Disable OpenMP dynamic control, HA-AL will govern this

    long long total_global_cycles = 0;
    long long total_global_instr = 0;
    long long total_global_l3_miss = 0;

    auto start = std::chrono::high_resolution_clock::now();
    
    bool changed = true;
    int iter = 0;
    
    while(changed && iter < 3) {
        changed = false;
        iter++;
        
        long long iter_cycles = 0;
        long long iter_instr = 0;
        long long iter_l3_miss = 0;

        // Apply thread limits based on PAPI heuristic feedback
        omp_set_num_threads(current_threads);
        
        #pragma omp parallel 
        {
            int EventSet = PAPI_NULL;
            PAPI_register_thread();
            
            bool papi_ok = false;
            if (PAPI_create_eventset(&EventSet) == PAPI_OK) {
                if (PAPI_add_event(EventSet, PAPI_TOT_CYC) == PAPI_OK &&
                    PAPI_add_event(EventSet, PAPI_TOT_INS) == PAPI_OK &&
                    PAPI_add_event(EventSet, PAPI_L3_TCM) == PAPI_OK) {
                    if (PAPI_start(EventSet) == PAPI_OK) papi_ok = true;
                }
            }

            std::vector<int> local_neighbors_count(g.num_nodes, 0);
            std::vector<int> local_unique_comms; 
            local_unique_comms.reserve(300); 

            #pragma omp for schedule(guided)
            for(int i = 0; i < g.num_nodes; i++) {
                if(g.adj[i].empty()) continue;

                int old_c = community[i];
                
                for(int n : g.adj[i]) {
                    int n_comm = community[n]; 
                    if(local_neighbors_count[n_comm] == 0) {
                        local_unique_comms.push_back(n_comm);
                    }
                    local_neighbors_count[n_comm]++;
                }

                int best_c = old_c;
                double max_gain = 0;

                for(int c : local_unique_comms) {
                    int k_in = local_neighbors_count[c];
                    double gain = (double)k_in - (double)(comm_weight[c] * g.degrees[i]) / m2;
                    if(gain > max_gain) {
                        max_gain = gain;
                        best_c = c;
                    }
                }

                if(best_c != old_c) {
                    // Topology-Aware Synchronization
                    if (comm_weight[old_c] > DENSITY_THRESHOLD) {
                        #pragma omp atomic
                        comm_weight[old_c] -= g.degrees[i];
                    } else {
                        omp_set_lock(&locks[old_c]);
                        comm_weight[old_c] -= g.degrees[i];
                        omp_unset_lock(&locks[old_c]);
                    }
                    
                    if (comm_weight[best_c] > DENSITY_THRESHOLD) {
                        #pragma omp atomic
                        comm_weight[best_c] += g.degrees[i];
                    } else {
                        omp_set_lock(&locks[best_c]);
                        comm_weight[best_c] += g.degrees[i];
                        omp_unset_lock(&locks[best_c]);
                    }
                    
                    community[i] = best_c;
                    changed = true; 
                }

                for(int c : local_unique_comms) {
                    local_neighbors_count[c] = 0;
                }
                local_unique_comms.clear();
            }

            if (papi_ok) {
                long long values[3] = {0, 0, 0}; 
                PAPI_stop(EventSet, values);
                PAPI_cleanup_eventset(EventSet);
                PAPI_destroy_eventset(&EventSet);

                #pragma omp atomic
                iter_cycles += values[0];
                #pragma omp atomic
                iter_instr += values[1];
                #pragma omp atomic
                iter_l3_miss += values[2];
            }
        } // End of parallel region

        total_global_cycles += iter_cycles;
        total_global_instr += iter_instr;
        total_global_l3_miss += iter_l3_miss;

        // --- HA-AL HEURISTIC FEEDBACK LOOP ---
        std::cout << ">> Iteration " << iter << " completed (" << current_threads << " Threads). L3 Misses: " << iter_l3_miss << std::endl;
        
        if (iter_l3_miss > L3_MISS_DANGER_ZONE && current_threads > 1) {
            current_threads = std::max(current_threads / 2, 1);
            std::cout << "   [HA-AL] Severe NUMA congestion detected! Throttling active threads to " << current_threads << "." << std::endl;
        } else if (iter_l3_miss < (L3_MISS_DANGER_ZONE / 2) && current_threads < omp_get_max_threads()) {
            current_threads = std::min(current_threads * 2, omp_get_max_threads());
            std::cout << "   [HA-AL] NUMA traffic stabilized. Scaling active threads up to " << current_threads << "." << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    #pragma omp parallel for schedule(static)
    for(int i = 0; i < g.num_nodes; i++) {
        omp_destroy_lock(&locks[i]);
    }
    delete[] locks;

    std::cout << "\n=== HA-AL FRAMEWORK FINAL RESULTS ===" << std::endl;
    std::cout << "Total Execution Time: " << elapsed.count() << " seconds." << std::endl;
    
    double final_ipc = (double)total_global_instr / (total_global_cycles > 0 ? total_global_cycles : 1);
    std::cout << "Final IPC Ratio: " << std::fixed << std::setprecision(4) << final_ipc << std::endl;

    // === MODULARITY (Q) QUALITY CHECK ===
    double modularity = 0.0;
    std::vector<long long> in_degree(g.num_nodes, 0);
    std::vector<long long> tot_degree(g.num_nodes, 0);

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < g.num_nodes; i++) {
        int c = community[i];
        #pragma omp atomic
        tot_degree[c] += g.degrees[i];

        for (int j : g.adj[i]) {
            if (community[i] == community[j]) {
                #pragma omp atomic
                in_degree[c] += 1; 
            }
        }
    }

    for (int i = 0; i < g.num_nodes; i++) {
        if (tot_degree[i] > 0) {
            double temp = (double)tot_degree[i] / (double)m2;
            modularity += (double)in_degree[i] / (double)m2 - (temp * temp);
        }
    }

    std::cout << "Algorithmic Quality (Modularity Q): " << std::fixed << std::setprecision(6) << modularity << std::endl;
    std::cout << "=====================================\n" << std::endl;
    
    delete[] community;
    delete[] comm_weight;
    
    return 0;
}