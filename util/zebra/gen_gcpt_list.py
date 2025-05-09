#!/usr/bin/env python3

import glob
import math
import os
import subprocess

import meta
import numpy as np


def gen_gcpt_list(benchmark):
    # 设置文件路径信息
    search_dir = f"{meta.NEMU_HOME}/parallel_result/checkpoint/{benchmark}"
    output_file = f"{meta.GEM5_HOME}/util/zebra/gcpt_list/{benchmark}.txt"

    # 根据物理核心数量设置进程batch_size
    batch_size = math.ceil(meta.interval_count[benchmark] / meta.core_num)

    # 找到所有数字命名的目录
    dirs = subprocess.check_output(
        ["find", search_dir, "-type", "d", "-name", "[0-9]*"], universal_newlines=True
    ).split("\n")[:-1]

    # 处理每个目录
    entries = []
    for dir in dirs:
        zstd_file = subprocess.check_output(
            ["find", dir, "-maxdepth", "1", "-name", "*.zstd"], universal_newlines=True
        ).strip()
        if zstd_file:
            part_number = os.path.basename(dir)
            entries.append(
                [f"part{part_number}", zstd_file, "20", str(batch_size * 20)]
            )

    # 根据第一列（part 编号）对条目进行排序
    entries.sort(key=lambda x: int(x[0][4:]))

    run_dir = f"{meta.GEM5_HOME}/util/zebra/run_info/{benchmark}"
    if os.path.exists(run_dir):
        run_list = sorted(glob.glob(f"{run_dir}/run*.txt"))
        run_data = np.array([np.loadtxt(file) for file in run_list], dtype=int)
        run_sum = np.sum(run_data, axis=0)
    else:
        run_sum = np.zeros(len(entries), dtype=int)

    # 将排序后的条目写入输出文件
    with open(output_file, "w") as outfile:
        for index, entry in enumerate(entries, start=1):
            entry[0] = f"part{index}"
            if run_sum[index - 1] >= batch_size:
                continue
            entry[3] = str((batch_size - run_sum[index - 1]) * 20)
            outfile.write(" ".join(entry) + "\n")


if __name__ == "__main__":
    # 设置 benchmark
    benchmark = meta.benchmarks[1]
    gen_gcpt_list(benchmark)
