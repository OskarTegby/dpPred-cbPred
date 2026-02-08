#!/bin/bash

# Export environment variables
export SNIPER_ROOT=$(pwd)
export GRAPHITE_ROOT=$(pwd)
export PIN_ROOT=$(pwd)/pin_kit
export PIN_HOME=$(pwd)/pin_kit
export BENCHMARKS_ROOT=$(pwd)/benchmarks

# Create directories if they don't exist
mkdir -p pin_kit
mkdir -p benchmarks

# Download and extract PIN 3.6
echo "Downloading PIN 3.6..."
wget https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.6-97554-g31f0a167d-gcc-linux.tar.gz

echo "Extracting PIN 3.6 to pin_kit..."
tar -xzf pin-3.6-97554-g31f0a167d-gcc-linux.tar.gz -C pin_kit --strip-components=1

# Clean up tarball
rm pin-3.6-97554-g31f0a167d-gcc-linux.tar.gz

# Clone benchmarks repository
echo "Cloning benchmarks repository..."
git clone https://github.com/OskarTegby/benchmarks benchmarks

echo "Done!"
echo "Environment variables set:"
echo "  SNIPER_ROOT=$SNIPER_ROOT"
echo "  GRAPHITE_ROOT=$GRAPHITE_ROOT"
echo "  PIN_ROOT=$PIN_ROOT"
echo "  PIN_HOME=$PIN_HOME"
echo "  BENCHMARKS_ROOT=$BENCHMARKS_ROOT"
