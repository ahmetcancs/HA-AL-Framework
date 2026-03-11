#!/bin/bash
cd "$(dirname "$0")/.."

echo "=== Running HA-AL Framework on All Datasets ==="
export OMP_NUM_THREADS=32

echo "------------------------------------------------"
echo "[TEST 1/3] Executing on web-Google dataset..."
./ha_al_executable web-Google.txt

echo "------------------------------------------------"
echo "[TEST 2/3] Executing on com-Youtube dataset..."
./ha_al_executable com-youtube.ungraph.txt

echo "------------------------------------------------"
echo "[TEST 3/3] Executing on roadNet-CA dataset..."
./ha_al_executable roadNet-CA.txt

echo "=== All benchmark experiments completed! ==="