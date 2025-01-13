#! /bin/bash

set -x

export GEM5_HOME=/root/xiangshan/GEM5  # The root of GEM5 project
export GEM5=$GEM5_HOME/build/RISCV/gem5.opt # GEM5 executable

export log_file="log.txt"
export output_dir=${GEM5_HOME}/output
mkdir -p $output_dir

export benchmark="gcc_166"


check() {
    if [ $1 -ne 0 ]; then
        echo FAIL
        touch abort
        exit
    fi
}
export -f check


function run() {
    set -x
    cpt=$1
    warmup_len=${2:-20000000}
    total_len=${3:-40000000}
    if [[ -n "$4" ]]; then
        work_dir=$4
    else
        work_dir=$PWD
    fi

    # used for touch completed or abort in $work_dir
    cd $work_dir

    # cpt_option related
    cpt_name=$(basename -- "$cpt")
    extension="${cpt_name##*.}"
    cpt_option="--generic-rv-cpt=$cpt --gcpt-restorer=$NEMU_HOME/resource/gcpt_restore/build/gcpt.bin"
    if [[ $extension != "gz" && $extension != "zstd" && $extension != "raw" ]]; then
        cpt_option="--generic-rv-cpt=$cpt --raw-cpt"
    fi

    # used for collect interval cycles 
    # for use, add $interval_cycle_args in below command line
    # interval_size=20000000
    # interval_cycle_file=${work_dir}/interval_cycle.txt
    # interval_cycle_args="--enable-interval-cycle --interval-size=${interval_size} --interval-cycle-file=${interval_cycle_file}"


    $GEM5 $GEM5_HOME/configs/example/fs.py \
        --xiangshan-system --cpu-type=DerivO3CPU \
        --mem-size=8GB \
        --caches --cacheline_size=64 \
        --l1i_size=64kB --l1i_assoc=8 \
        --l1d_size=64kB --l1d_assoc=8 \
        --l1d-hwp-type=XSCompositePrefetcher \
        --short-stride-thres=0 \
        --l2cache --l2_size=1MB --l2_assoc=8 \
        --l3cache --l3_size=16MB --l3_assoc=16 \
        --l1-to-l2-pf-hint \
        --l2-hwp-type=WorkerPrefetcher \
        --l2-to-l3-pf-hint \
        --l3-hwp-type=WorkerPrefetcher \
        --mem-type=DRAMsim3 \
        --dramsim3-ini=$GEM5_HOME/ext/dramsim3/xiangshan_configs/xiangshan_DDR4_8Gb_x8_3200_2ch.ini \
        --bp-type=DecoupledBPUWithFTB --enable-loop-predictor \
        $cpt_option \
        --warmup-insts-no-switch=$warmup_len \
        --maxinsts=$total_len
    check $?

    touch completed
}
export -f run



function single_run() {
    work_dir=${output_dir}/single
    rm -rf $work_dir
    mkdir -p $work_dir

    warmup_inst=$(( 20 * 10**6 ))
    max_inst=$(( 20 * 10**6 ))

    workload=/root/xiangshan/NEMU/ready-to-run/linux.bin
    run $workload $warmup_inst $max_inst $work_dir > $work_dir/$log_file 2>&1
}
export -f single_run



function prepare_env() {
    set -x
    echo "prepare_env $@"
    all_args=("$@")
    task=${all_args[0]}
    workload=${all_args[1]}

    work_dir=$output_dir/cycles/$benchmark/$task
    echo $work_dir
    rm -rf $work_dir
    mkdir -p $work_dir
}
export -f prepare_env



function arg_wrapper() {
    prepare_env $@

    all_args=("$@")
    args=(${all_args[0]})

    k=1000
    M=$((1000 * $k))

    warmup=${args[2]}
    sample=${args[3]}

    warmup_len=$(( $warmup * $M ))
    total_len=$(( ($warmup + $sample) * $M ))

    run $workload $warmup_len $total_len $work_dir > $work_dir/$log_file 2>&1
}
export -f arg_wrapper


export workload_list=/home/xiongtianyu/xs/gem5/util/zebra/workload_list/${benchmark}.txt
function parallel_run() {
    # We use gnu parallel to control the parallelism.
    # If your server has 32 core and 64 SMT threads, we suggest to run with no more than 32 threads.
    export num_threads=24
    cat $workload_list | parallel -a - -j $num_threads arg_wrapper {}
}
export -f parallel_run

single_run
# parallel_run
