Hardware-Aware Adaptive Louvain (HA-AL) Framework

This repository contains the anonymized C++ source code for the Hardware-Aware Adaptive Louvain (HA-AL) framework. This framework is designed to mitigate the "Memory Wall" and lock contention bottlenecks in parallel graph processing on NUMA architectures.

HA-AL dynamically toggles between hardware-supported atomic operations and fine-grained OpenMP locks based on real-time topological density. Furthermore, it employs a PAPI-driven heuristic feedback loop to monitor L3 cache misses and autonomously throttle the active thread count to prevent memory bus saturation.

🛠️ Prerequisites and Dependencies

To compile and execute the HA-AL framework, the following dependencies must be installed on a Linux-based environment (tested on Ubuntu 20.04/22.04 LTS):

GCC Compiler: Version 7.5 or higher with OpenMP support.

PAPI Library: Performance Application Programming Interface (libpapi-dev).

Important Note for PAPI Hardware Counters: To allow PAPI to read hardware performance counters without root privileges, you must disable the kernel's paranoid constraint before execution:

sudo sysctl -w kernel.perf_event_paranoid=-1


📊 Datasets

We evaluate HA-AL using standard real-world graphs from the Stanford Network Analysis Project (SNAP). Run the provided script to automatically download and extract all three evaluation datasets (web-Google, com-Youtube, and roadNet-CA):

bash scripts/download_datasets.sh


⚙️ Compilation & Execution

Compile the code using the -O3 optimization flag and link the OpenMP and PAPI libraries by executing the build script:

bash scripts/build.sh


Before running the executable, the script will export the maximum number of OpenMP threads available on your hardware. HA-AL will dynamically scale this number down if NUMA congestion is detected. Run the full benchmark suite with:

bash scripts/run_experiments.sh


📈 Expected Output

Upon successful execution, the framework will output the dynamic thread scaling actions taken during each algorithmic iteration, followed by the final execution metrics (Total Time, IPC Ratio, and Modularity Q). Example behavior:

>> Iteration 1 completed (32 Threads). L3 Misses: 11512304
   [HA-AL] Severe NUMA congestion detected! Throttling active threads to 16.
>> Iteration 2 completed (16 Threads). L3 Misses: 5074211
   [HA-AL] NUMA traffic stabilized. Scaling active threads up to 32.

=== HA-AL FRAMEWORK FINAL RESULTS ===
Total Execution Time: 0.154 seconds.
Final IPC Ratio: 0.1490
Algorithmic Quality (Modularity Q): 0.763102
=====================================


🔒 Double-Blind Peer Review

This repository has been strictly anonymized for the double-blind peer review process. All author names, affiliations, and identifying metadata have been removed from the source code and commit history.
