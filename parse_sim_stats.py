#!/usr/bin/env python3
"""Parse simulation statistics and calculate IPC."""

import re
import sys


def parse_sim_output(filename):
    """Parse simulation output and return totals."""
    with open(filename, 'r') as f:
        content = f.read()
    
    # Extract instructions and cycles
    instructions = sum(map(int, re.search(r'Instructions\s+\|\s+(\d+)\s+\|\s+(\d+)', content).groups()))
    reported_cycles = sum(map(int, re.search(r'Cycles\s+\|\s+(\d+)\s+\|\s+(\d+)', content).groups()))

    
    # Extract cache stats for specified level
    def get_cache_stats(level):
        pattern = rf'Cache {level}.*?num cache accesses\s+\|\s+(\d+)\s+\|\s+(\d+).*?num cache misses\s+\|\s+(\d+)\s+\|\s+(\d+).*?predictor llc accesses\s+\|\s+(\d+)\s+\|\s+(\d+).*?predictor llc bypass misses\s+\|\s+(\d+)\s+\|\s+(\d+)'
        match = re.search(pattern, content, re.DOTALL)
        accesses = int(match.group(1)) + int(match.group(2))
        misses = int(match.group(3)) + int(match.group(4))
        pred_accesses = int(match.group(5)) + int(match.group(6))
        pred_misses = int(match.group(7)) + int(match.group(8))
        return accesses, misses, pred_accesses, pred_misses
    
    return instructions, reported_cycles, get_cache_stats


def calculate_ipc(instructions, reported_cycles, llc_accesses, llc_misses, pred_accesses, pred_misses, miss_penalty, hit_latency):
    """Calculate IPC using the specified formula."""
    llc_hits = llc_accesses - llc_misses
    pred_hits = pred_accesses - pred_misses
    
    estimated_cycles = (reported_cycles - llc_misses * miss_penalty - llc_hits * hit_latency
                                        + pred_misses * miss_penalty + pred_hits * hit_latency)
    print(f"estimated_cycles = {estimated_cycles}")
    
    return instructions / estimated_cycles

def main():
    def_hit_lat = 40
    def_miss_pen = 191

    if len(sys.argv) < 2:
        print("Usage: python parse_sim_stats.py <sim_output_file> [miss_penalty] [hit_latency] [cache_level]")
        print(f"Defaults: miss_penalty={def_miss_pen}, hit_latency={def_hit_lat}, cache_level=L3")
        sys.exit(1)
    
    filename = sys.argv[1]
    miss_penalty = float(sys.argv[2]) if len(sys.argv) > 2 else def_miss_pen
    hit_latency = float(sys.argv[3]) if len(sys.argv) > 3 else def_hit_lat 
    cache_level = sys.argv[4] if len(sys.argv) > 4 else 'L3'
    
    # Parse file
    instructions, reported_cycles, get_cache_stats = parse_sim_output(filename)
    llc_accesses, llc_misses, pred_accesses, pred_misses = get_cache_stats(cache_level)

    print(f"reported_cycles = {reported_cycles}")
    
    # Calculate IPC
    ipc = calculate_ipc(instructions, reported_cycles, llc_accesses, llc_misses, 
                        pred_accesses, pred_misses, miss_penalty, hit_latency)
    
    # Print results
    print(f"Totals: Instructions={instructions}, Cycles={reported_cycles}")
    print(f"{cache_level}: Accesses={llc_accesses}, Misses={llc_misses}, Hits={llc_accesses-llc_misses}")
    print(f"Predictor: Accesses={pred_accesses}, Misses={pred_misses}, Hits={pred_accesses-pred_misses}")
    print(f"\nIPC (miss_penalty={miss_penalty}, hit_latency={hit_latency}): {ipc:.6f}")


if __name__ == '__main__':
    main()
