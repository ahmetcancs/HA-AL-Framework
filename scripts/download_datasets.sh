#!/bin/bash
cd "$(dirname "$0")/.."

echo "=== Downloading Datasets from Stanford SNAP ==="

echo "-> 1. Downloading web-Google..."
wget https://snap.stanford.edu/data/web-Google.txt.gz
gunzip -f web-Google.txt.gz

echo "-> 2. Downloading com-Youtube..."
wget https://snap.stanford.edu/data/bigdata/communities/com-youtube.ungraph.txt.gz
gunzip -f com-youtube.ungraph.txt.gz

echo "-> 3. Downloading roadNet-CA..."
wget https://snap.stanford.edu/data/roadNet-CA.txt.gz
gunzip -f roadNet-CA.txt.gz

echo "=== All datasets downloaded and extracted successfully! ==="