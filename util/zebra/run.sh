#! /bin/bash

set -x

# The root of GEM5 project
export GEM5_HOME=/root/GEM5
# The path of GEM5 executable
export GEM5=$GEM5_HOME/build/RISCV/gem5.opt
# The name of the log file
export log_file="log.txt"

# The output directory for the simulation results
export output_dir=${GEM5_HOME}/output
mkdir -p $output_dir

# Set the benchmark name
export benchmark="libquantum"

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

	# Used for touch completed or abort in $work_dir
	cd $work_dir

	# cpt_option related
	cpt_name=$(basename -- "$cpt")
	extension="${cpt_name##*.}"
	cpt_option="--generic-rv-cpt=$cpt --gcpt-restorer=$NEMU_HOME/resource/gcpt_restore/build/gcpt.bin"
	if [[ $extension != "gz" && $extension != "zstd" && $extension != "raw" ]]; then
		cpt_option="--generic-rv-cpt=$cpt --raw-cpt"
	fi

	# Used for collect interval cycles
	# For use, add $interval_cycle_args in below command line
	interval_size=20000000
	interval_cycle_file=${work_dir}/interval_cycle.txt
	interval_cycle_args="--enable-interval-cycle --interval-size=${interval_size} --interval-cycle-file=${interval_cycle_file}"

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
		$interval_cycle_args \
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

	warmup_inst=$((20 * 10 ** 6))
	max_inst=$((40 * 10 ** 6))

	workload=/root/NEMU/ready-to-run/linux.bin
	run $workload $warmup_inst $max_inst $work_dir >$work_dir/$log_file 2>&1
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

	warmup_len=$(($warmup * $M))
	total_len=$((($warmup + $sample) * $M))

	run $workload $warmup_len $total_len $work_dir >$work_dir/$log_file 2>&1
}
export -f arg_wrapper

export workload_list=${GEM5_HOME}/util/zebra/gcpt_list/${benchmark}.txt
function parallel_run() {
	# We use gnu parallel to control the parallelism.
	# If your server has 32 core and 64 SMT threads, we suggest to run with no more than 32 threads.
	export num_threads=24
	cat $workload_list | parallel -a - -j $num_threads arg_wrapper {}
}
export -f parallel_run

# Usually, I use parallel_run to simulate benchmark, and use single_run to debug.

# single_run
parallel_run

# workload_list contains four columns:
# 1. task name
# 2. workload path
# 3. warmup length in M
# 4. sample length in M
: <<workload_list_example
part1 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/1624/_1624_0.000000_.zstd 20 52120
part2 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/5853/_5853_0.000000_.zstd 20 52140
part3 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/10092/_10092_0.000000_.zstd 20 51960
part4 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/14342/_14342_0.000000_.zstd 20 51560
part5 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/18579/_18579_0.000000_.zstd 20 51420
part6 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/22795/_22795_0.000000_.zstd 20 51700
part7 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/27042/_27042_0.000000_.zstd 20 51360
part8 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/31263/_31263_0.000000_.zstd 20 51540
part9 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/35485/_35485_0.000000_.zstd 20 51700
part10 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/39706/_39706_0.000000_.zstd 20 51880
part11 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/43929/_43929_0.000000_.zstd 20 52020
part12 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/48158/_48158_0.000000_.zstd 20 52040
part13 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/52397/_52397_0.000000_.zstd 20 51860
part14 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/56639/_56639_0.000000_.zstd 20 51620
part15 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/60881/_60881_0.000000_.zstd 20 51380
part16 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/65099/_65099_0.000000_.zstd 20 51620
part17 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/69328/_69328_0.000000_.zstd 20 51640
part18 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/73562/_73562_0.000000_.zstd 20 51560
part19 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/77780/_77780_0.000000_.zstd 20 51800
part20 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/82004/_82004_0.000000_.zstd 20 51920
part21 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/86243/_86243_0.000000_.zstd 20 51740
part22 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/90476/_90476_0.000000_.zstd 20 51680
part23 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/94712/_94712_0.000000_.zstd 20 51560
part24 /root/xiangshan/NEMU/parallel_result/checkpoint/libquantum/98939/_98939_0.000000_.zstd 20 51620
workload_list_example

# The above workload_list_example is used to collect the interval cycles of libquantum benchmark.
# Our machine has 24 physical cores, so we set num_threads=24.
