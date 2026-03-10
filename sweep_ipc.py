#!/usr/bin/env python3
"""
Sweep all result directories, compute IPC for each benchmark,
and print a separate speedup table per benchmark.

LLC predictor (cbPred) detection: directory name contains 'bthd'.
When cbPred is active, reported cycles are wrong; we use the
corrected cycle estimate from parse_sim_output().
When cbPred is inactive, we use Instructions/Cycles directly from sim.out.
"""

import re
import sys
from pathlib import Path

# ── tunables ────────────────────────────────────────────────────────────────
MISS_PENALTY = 191
HIT_LATENCY  = 40
CACHE_LEVEL  = "L3"
BENCHMARKS   = ["parsec-canneal", "npb-cg"]
SIM_FILE     = "sim.out"
# ────────────────────────────────────────────────────────────────────────────


def parse_sim_out(path: Path):
    """
    Parse a sim.out file.

    Returns (instructions, cycles, get_cache_stats) where:
      - instructions are summed over all cores
      - cycles is the max across cores (wall-clock; all active cores identical,
        but max avoids picking up Core 0 if it exits early as the manager thread)
      - get_cache_stats(level) returns (accesses, misses, pred_accesses, pred_misses)
        with per-core cache traffic summed and global predictor stats taken once.
    """
    content = path.read_text()

    def row_sum(label):
        """Sum all integer columns for a row matching `label`."""
        m = re.search(rf'{re.escape(label)}\s*\|(.+)', content)
        if not m:
            raise ValueError(f"Row not found: {label!r} in {path}")
        return sum(int(x) for x in re.findall(r'\d+', m.group(1)))

    def row_first(label):
        """Return the first (Core 0) integer value for a row."""
        m = re.search(rf'{re.escape(label)}\s*\|(.+)', content)
        if not m:
            raise ValueError(f"Row not found: {label!r} in {path}")
        vals = re.findall(r'\d+', m.group(1))
        if not vals:
            raise ValueError(f"No integers in row: {label!r}")
        return int(vals[0])

    instructions = row_sum("Instructions")

    # Wall-clock cycles: all active cores report the same value; take max to
    # avoid picking up Core 0 if it exits early as the manager thread.
    m_cyc = re.search(r'Cycles\s*\|(.+)', content)
    cycles = max(int(x) for x in re.findall(r'\d+', m_cyc.group(1)))

    def get_cache_stats(level):
        # Find the block for this cache level
        pattern = (
            rf'Cache {level}\s*\|.*?'
            rf'num cache accesses\s*\|(.+?)\n.*?'
            rf'num cache misses\s*\|(.+?)\n.*?'
            rf'predictor llc accesses\s*\|(.+?)\n.*?'
            rf'predictor llc bypass misses\s*\|(.+?)\n'
        )
        m = re.search(pattern, content, re.DOTALL)
        if not m:
            raise ValueError(f"Cache {level} block not found in {path}")

        accesses      = sum(int(x) for x in re.findall(r'\d+', m.group(1)))
        misses        = sum(int(x) for x in re.findall(r'\d+', m.group(2)))
        # Predictor stats are global — duplicated across cores; take Core 0 value only.
        pred_accesses = int(re.findall(r'\d+', m.group(3))[0])
        pred_misses   = int(re.findall(r'\d+', m.group(4))[0])

        return accesses, misses, pred_accesses, pred_misses

    return instructions, cycles, get_cache_stats


def ipc_direct(instructions, cycles):
    return instructions / cycles


def ipc_corrected(instructions, cycles, llc_accesses, llc_misses,
                  pred_accesses, pred_misses, miss_penalty, hit_latency):
    llc_hits  = llc_accesses - llc_misses
    pred_hits = pred_accesses - pred_misses
    estimated_cycles = (
        cycles
        - llc_misses  * miss_penalty - llc_hits  * hit_latency
        + pred_misses * miss_penalty + pred_hits * hit_latency
    )
    return instructions / estimated_cycles


def cbpred_enabled(dir_name: str) -> bool:
    return "bthd" in dir_name


def collect_results(root: Path, benchmark: str):
    """
    Walk `root` for directories containing `benchmark`/SIM_FILE.
    Returns a list of dicts with keys: dir, ipc, method.
    """
    results = []
    for entry in sorted(root.iterdir()):
        if not entry.is_dir():
            continue
        sim_path = entry / benchmark / SIM_FILE
        if not sim_path.exists():
            continue

        try:
            instructions, cycles, get_cache_stats = parse_sim_out(sim_path)
        except Exception as e:
            print(f"  [WARN] {entry.name}/{benchmark}: parse error — {e}", file=sys.stderr)
            continue

        if cbpred_enabled(entry.name):
            try:
                llc_acc, llc_mis, pred_acc, pred_mis = get_cache_stats(CACHE_LEVEL)
            except Exception as e:
                print(f"  [WARN] {entry.name}/{benchmark}: cache parse error — {e}", file=sys.stderr)
                continue
            ipc = ipc_corrected(instructions, cycles, llc_acc, llc_mis,
                                 pred_acc, pred_mis, MISS_PENALTY, HIT_LATENCY)
            method = "corrected"
        else:
            ipc    = ipc_direct(instructions, cycles)
            method = "direct"

        results.append({"dir": entry.name, "ipc": ipc, "method": method})

    return results


def print_table(results, benchmark):
    if not results:
        print(f"No results found for {benchmark}.", file=sys.stderr)
        return

    # Find baseline IPC
    baseline = next((r for r in results if r["dir"].startswith("baseline")), None)
    if baseline is None:
        print(f"No baseline_* directory found for {benchmark}.", file=sys.stderr)
        return
    baseline_ipc = baseline["ipc"]

    # Print table
    col_w = max(len(r["dir"]) for r in results)
    header = f"{'Directory':<{col_w}}  {'IPC':>10}  {'Speedup':>9}  {'Method'}"
    print(f"\n{'─' * len(header)}")
    print(f"Benchmark: {benchmark}")
    print('─' * len(header))
    print(header)
    print("-" * len(header))

    for r in results:
        speedup = r["ipc"] / baseline_ipc
        marker  = " <-- baseline" if r["dir"] == baseline["dir"] else ""
        print(f"{r['dir']:<{col_w}}  {r['ipc']:>10.6f}  {speedup:>8.4f}x  {r['method']}{marker}")


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

    for benchmark in BENCHMARKS:
        results = collect_results(root, benchmark)
        print_table(results, benchmark)


if __name__ == "__main__":
    main()
