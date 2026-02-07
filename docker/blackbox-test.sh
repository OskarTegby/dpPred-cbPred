#!/bin/bash

set -e

# Build
make all >/dev/null 2>&1

# Start container in detached mode
CONTAINER_ID=$(make run-detached)

# Cleanup on exit
trap "docker stop $CONTAINER_ID > /dev/null 2>&1" EXIT

cd ..

filter_output() {
  local file=$1
  local extra_vars=$2

  # Build the filter pattern
  local pattern="(Time \(ns\)|Idle time|Cycles|num correct|num incorrect"

  # Add extra_vars if provided
  if [ -n "$extra_vars" ]; then
    pattern="${pattern}|${extra_vars}"
  fi

  pattern="${pattern})"

  # Filter out unwanted lines and remove % signs
  grep -v -E "$pattern" "$file" | sed 's/%//g'
}

get_filter_for_benchmark() {
  local benchmark="$1"
  local result

  case "$benchmark" in
  "parsec-canneal")
    result="num_allocs"
    ;;
  "splash2-barnes")
    result="num accesses|num misses|num allocs|num cache accesses|num cache misses|num loads from dram"
    ;;
  "npb-cg")
    result="predictor llc default misses|num cache misses|num loads from dram"
    ;;
  *)
    result=""
    ;;
  esac

  printf '%s' "$result"
}

run_test() {
  local benchmark=$1
  local cores=$2
  local abs_tol=$3
  local rel_tol=$4

  echo "Running test: $benchmark"

  # Use absolute path to benchmarks directory
  docker exec -w "${PWD}" $CONTAINER_ID \
    ./benchmarks/run-sniper -p "$benchmark" -n "$cores" >/dev/null

  local temp_output="./sim.out"
  local expected_file="./benchmarks/expected_outputs/${benchmark}.out"

  local filtered_temp="/tmp/${benchmark}_filtered_temp"
  local filtered_expected="/tmp/${benchmark}_filtered_expected"

  local filter
  filter="$(get_filter_for_benchmark $benchmark)"

  echo "${filtern}"

  filter_output "$temp_output" "$filter" >"$filtered_temp"
  filter_output "$expected_file" "$filter" >"$filtered_expected"

  if numdiff -a "$abs_tol" -r "$rel_tol" "$filtered_temp" "$filtered_expected" >/dev/null 2>&1; then
    echo "✓ PASSED: $benchmark (output matches expected)"
  else
    echo "✗ FAILED: $benchmark (output differs from expected)"
    echo ""
    echo "=== Diff ==="
    numdiff -a "$abs_tol" -r "$rel_tol" "$filtered_temp" "$filtered_expected" || true
    echo "============"
    diff "$filtered_temp" "$filtered_expected" || true
    echo "============"
    echo ""
  fi
}

# Run your tests
run_test "parsec-canneal" 2 5 0.2
run_test "splash2-barnes" 2 5 0.1
run_test "npb-cg" 2 7 0.05
