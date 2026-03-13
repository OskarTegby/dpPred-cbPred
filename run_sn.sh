#!/bin/bash

REPO="$HOME/dpPred-cbPred"
SNIPER="$REPO/benchmarks/run-sniper"
PREDICTOR_CONFIG="$REPO/benchmarks/predictor_config.txt"
RESULTS_BASE="$REPO/results"
JOBS_DIR="$RESULTS_BASE/jobs"
BENCHMARKS=("parsec-canneal" "npb-cg")

# Default predictor config values
PFQ_SIZE=8
SHADOW_TABLE_SIZE=2

# Per-benchmark core counts
declare -A BENCHMARK_CORES
BENCHMARK_CORES["parsec-canneal"]=2
BENCHMARK_CORES["npb-cg"]=1

# Parse command line args
DEBUG=0
while getopts "d" opt; do
  case $opt in
  d) DEBUG=1 ;;
  esac
done

mkdir -p "$JOBS_DIR"

submit_job() {
  local exp_name=$1
  local dppred=$2
  local cbpred=$3
  local phist_thd=$4
  local bhist_thd=$5
  local job_script="$JOBS_DIR/${exp_name}.sh"

  if [ "$DEBUG" -eq 1 ]; then
    local CANNEAL_INPUT="simsmall"
    local NPB_INPUT="W"
    local WALLTIME="06:00:00"
    exp_name="debug_${exp_name}"
  else
    local CANNEAL_INPUT="simmedium"
    local NPB_INPUT="A"
    local WALLTIME="24:00:00"
  fi

  cat >"$job_script" <<EOF
#!/bin/bash
#$ -N ${exp_name}
#$ -o $JOBS_DIR/${exp_name}.stdout
#$ -e $JOBS_DIR/${exp_name}.stderr
#$ -l h_rt=${WALLTIME}
ulimit -c 0

declare -A BENCHMARK_INPUTS
BENCHMARK_INPUTS["parsec-canneal"]="${CANNEAL_INPUT}"
BENCHMARK_INPUTS["npb-cg"]="${NPB_INPUT}"
declare -A BENCHMARK_CORES
BENCHMARK_CORES["parsec-canneal"]=${BENCHMARK_CORES["parsec-canneal"]}
BENCHMARK_CORES["npb-cg"]=${BENCHMARK_CORES["npb-cg"]}

# Set unique predictor config path for this job
RUN_ID="${exp_name}_\${JOB_ID}"
RUN_DIR="$RESULTS_BASE/\$RUN_ID"

export PREDICTOR_CONFIG="\$RUN_DIR/predictor_config.txt"
mkdir -p "\$RUN_DIR"

# Write predictor config
cat > "\$PREDICTOR_CONFIG" << PREDCFG
DPPRED=$dppred
CBPRED=$cbpred
PHIST_THD=$phist_thd
BHIST_THD=$bhist_thd
PFQ_SIZE=$PFQ_SIZE
SHADOW_TABLE_SIZE=$SHADOW_TABLE_SIZE
PREDCFG

echo "Running experiment: $exp_name"
echo "DPPRED=$dppred CBPRED=$cbpred PHIST_THD=$phist_thd BHIST_THD=$bhist_thd"

# Run all benchmarks sequentially within this job
for benchmark in ${BENCHMARKS[@]}; do
    output_dir="\$RUN_DIR/\${benchmark}"
    mkdir -p "\$output_dir"

    cat > "\${output_dir}/run_info.txt" << INFO
exp_name=$exp_name
benchmark=\$benchmark
input=\${BENCHMARK_INPUTS[\$benchmark]}
dppred=$dppred
cbpred=$cbpred
phist_thd=$phist_thd
bhist_thd=$bhist_thd
pfq_size=$PFQ_SIZE
shadow_table_size=$SHADOW_TABLE_SIZE
INFO
    echo "  Running \$benchmark with \${BENCHMARK_CORES[\$benchmark]} cores..."
    $SNIPER -c run.cfg -i "\${BENCHMARK_INPUTS[\$benchmark]}" -p "\$benchmark" \
            -n "\${BENCHMARK_CORES[\$benchmark]}" -d "\$output_dir" \
            > "\${output_dir}/stdout.log" 2> "\${output_dir}/stderr.log"
done

echo "Experiment $exp_name complete."
EOF

  chmod +x "$job_script"
  qsub "$job_script"
  echo "Submitted: $exp_name"
}

if [ "$DEBUG" -eq 1 ]; then
  submit_job "baseline" 0 0 0 0
  submit_job "pthd6_bthd6" 1 1 6 6
else
  # Baseline: both predictors off
  submit_job "baseline" 0 0 0 0

  # dppred only: vary phist_thd from 0 to 6, cbpred=0
  for thd in $(seq 0 6); do
    submit_job "pthd${thd}" 1 0 $thd 0
  done

  # cbpred with dppred: vary both thresholds
  for phist in $(seq 0 6); do
    for bhist in $(seq 0 6); do
      submit_job "pthd${phist}_bthd${bhist}" 1 1 $phist $bhist
    done
  done
fi
