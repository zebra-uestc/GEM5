#!/usr/bin/env python3

import os
import subprocess

import meta


def get_line_count(benchmark, run_id):
    # 指定搜索路径
    run_dir = f"{base_dir}/{benchmark}/run{run_id}"

    output_file = f"{out_dir}/{benchmark}/run{run_id}.txt"
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    # 打开结果文件，准备写入
    with open(output_file, "w") as outfile:
        # 遍历目录
        for i in range(1, 25):
            # 构建子目录路径
            part_dir = f"{run_dir}/part{i}"

            # 构建文件路径
            cycle_file = f"{part_dir}/interval_cycle.txt"

            # 使用wc命令统计行数
            ret = subprocess.check_output(["wc", "-l", cycle_file])
            ret_str = ret.decode("utf-8").strip()

            # 分割输出并获取行数
            line_count = int(ret_str.split()[0])

            # 写入结果文件
            outfile.write(str(line_count) + "\n")


if __name__ == "__main__":
    benchmark = meta.benchmarks[1]
    # 设置基本信息
    base_dir = f"{meta.GEM5_HOME}/output/cycles"
    out_dir = f"{meta.GEM5_HOME}/util/zebra/run_info"
    run_id = 1
    get_line_count(benchmark, run_id)
