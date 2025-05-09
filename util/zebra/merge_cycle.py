#!/bin/env python3

import math
import os

import meta


def count_subdirs(path):
    count = 0
    for item in os.listdir(path):
        item_path = os.path.join(path, item)
        if os.path.isdir(item_path):
            count += 1
    return count


def merge_cycles(benchmark):
    bench_dir = f"{base_dir}/{benchmark}"
    output_file = f"{bench_dir}/cycle.txt"

    run_times = count_subdirs(bench_dir)
    print(f"{benchmark} run times: {run_times}")

    batch_size = math.ceil(meta.interval_count[benchmark] / meta.core_num)
    total_line_count = 0
    with open(output_file, "w") as outfile:
        for part_id in range(1, meta.core_num + 1):
            line_count = 0
            for run_id in range(1, run_times + 1):
                part_dir = f"{bench_dir}/run{run_id}/part{part_id}"
                if not os.path.exists(part_dir):
                    print(f"Directory {part_dir} does not exist.")
                    continue

                cycle_file = f"{part_dir}/interval_cycle.txt"
                with open(cycle_file, "r") as infile:
                    for line in infile:
                        outfile.write(line)
                        line_count += 1
                        if line_count == batch_size:
                            break
            print("==> part_id: ", part_id, "line_count: ", line_count)
            total_line_count += line_count

    print(f"total_line_count: {total_line_count}")


if __name__ == "__main__":
    benchmark = meta.benchmarks[0]
    # 设置基本信息
    base_dir = f"{meta.GEM5_HOME}/output/cycles"
    merge_cycles(benchmark)
