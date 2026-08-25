#!/usr/bin/env python3
import csv
import statistics
import sys
from collections import defaultdict


def median(values):
    return statistics.median(values) if values else 0.0


def load_rows(paths):
    rows = []
    for path in paths:
        with open(path, newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                if int(row["rc"]) != 0:
                    continue
                rows.append(row)
    return rows


def summarize(rows):
    groups = defaultdict(list)
    for row in rows:
        key = (row["tree"], row["callback"], row["model"], int(row["threads"]))
        groups[key].append(row)

    summary = {}
    for key, group_rows in groups.items():
        wall = [float(r["wall_ms"]) for r in group_rows]
        throughput = [float(r["throughput"]) for r in group_rows]
        cpu_util = [float(r["cpu_util"]) for r in group_rows]
        vol_cs = [float(r["vol_cs"]) for r in group_rows]
        invol_cs = [float(r["invol_cs"]) for r in group_rows]
        entries = [int(r["entries"]) for r in group_rows]
        summary[key] = {
            "runs": len(group_rows),
            "entries": int(median(entries)),
            "wall_ms": median(wall),
            "throughput": median(throughput),
            "cpu_util": median(cpu_util),
            "vol_cs": median(vol_cs),
            "invol_cs": median(invol_cs),
        }
    return summary


def print_tables(summary):
    trees = sorted({k[0] for k in summary})
    callbacks = sorted({k[1] for k in summary})
    models = sorted({k[2] for k in summary})

    for tree in trees:
        for callback in callbacks:
            keys = [k for k in summary if k[0] == tree and k[1] == callback]
            if not keys:
                continue

            print(f"## {tree} / {callback}")
            print()
            print("| model | threads | runs | entries | wall_ms | speedup | throughput | cpu_util | vol_cs | invol_cs |")
            print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")

            for model in models:
                model_threads = sorted(k[3] for k in keys if k[2] == model)
                if not model_threads:
                    continue
                base_key = (tree, callback, model, model_threads[0])
                base_wall = summary[base_key]["wall_ms"]
                for threads in model_threads:
                    key = (tree, callback, model, threads)
                    s = summary[key]
                    speedup = base_wall / s["wall_ms"] if s["wall_ms"] > 0 else 0.0
                    print(
                        f"| {model} | {threads} | {s['runs']} | {s['entries']} | "
                        f"{s['wall_ms']:.3f} | {speedup:.2f} | {s['throughput']:.1f} | "
                        f"{s['cpu_util']:.2f} | {s['vol_cs']:.0f} | {s['invol_cs']:.0f} |"
                    )
            print()

            best = min(keys, key=lambda k: summary[k]["wall_ms"])
            best_s = summary[best]
            print(
                f"Best: model={best[2]}, threads={best[3]}, "
                f"wall_ms={best_s['wall_ms']:.3f}, throughput={best_s['throughput']:.1f}"
            )
            print()


def main(argv):
    if len(argv) < 2:
        print(f"Usage: {argv[0]} RESULTS.csv [RESULTS.csv ...]", file=sys.stderr)
        return 1
    rows = load_rows(argv[1:])
    if not rows:
        print("No successful benchmark rows found", file=sys.stderr)
        return 1
    print_tables(summarize(rows))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
