#!/bin/bash
cd "$(dirname "$0")/.."

echo "=== Compiling HA-AL Framework ==="
g++ -O3 -fopenmp src/main_final.cpp -o ha_al_executable -lpapi
echo "Compilation successful! Executable created in the root directory as 'ha_al_executable'"