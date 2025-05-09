NEMU_HOME = "/root/NEMU"
GEM5_HOME = "/root/GEM5"

core_num = 24

# 1st batch
""""
benchmarks = [
    "perlbench_checkspam",
    "bzip2_chicken",
    "gcc_166",
    "bwaves",
    "gamess_cytosine",
    "milc",
    "gobmk_13x13",
    "hmmer_nph3",
]

interval_count = {
    "perlbench_checkspam": 54819,
    "bzip2_chicken": 9040,
    "gcc_166": 3694,
    "bwaves": 65381,
    "gamess_cytosine": 63236,
    "milc": 39882,
    "gobmk_13x13": 10989,
    "hmmer_nph3": 56020,
}
"""

# 2nd batch
benchmarks = [
    "sjeng",
    "libquantum",
    "h264ref_foreman.baseline",
    "omnetpp",
    "astar_rivers",
    "xalancbmk",
]
interval_count = {
    "sjeng": 122020,
    "libquantum": 101497,
    "h264ref_foreman.baseline": 26291,
    "omnetpp": 23630,
    "astar_rivers": 29068,
    "xalancbmk": 39383,
}
