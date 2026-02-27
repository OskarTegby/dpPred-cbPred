#!/bin/bash

REPO="$HOME/dpPred-cbPred"
SNIPER="$REPO/benchmarks/run-sniper"
PREDICTOR_CONFIG="$REPO/benchmarks/predictor_config.txt"
RESULTS_BASE="$REPO/results"
JOBS_DIR="$RESULTS_BASE/jobs"
BENCHMARKS=("parsec-canneal" "npb-cg")
CORES=4

# Default predictor config values
PFQ_SIZE=8
SHADOW_TABLE_SIZE=2

mkdir -p "$JOBS_DIR"

submit_job() {
  local exp_name=$1
  local dppred=$2
  local cbpred=$3
  local phist_thd=$4
  local bhist_thd=$5
  local job_script="$JOBS_DIR/${exp_name}.sh"

  cat >"$job_script" <<EOF
#!/bin/bash
#$ -N ${exp_name}
#$ -o $JOBS_DIR/${exp_name}.stdout
#$ -e $JOBS_DIR/${exp_name}.stderr
#$ -l h_rt=24:00:00

declare -A BENCHMARK_INPUTS
BENCHMARK_INPUTS["parsec-canneal"]="simmedium"
BENCHMARK_INPUTS["npb-cg"]="A"

# Set unique predictor config path for this job
RUN_ID="${exp_name}_\${JOB_ID}"
RUN_DIR="$RESULTS_BASE/\$RUN_ID"

export PREDICTOR_CONFIG="\$RUN_DIR/predictor_config.txt"
mkdir -p "\$RUN_DIR"

# Write predictor config
cat > "$PREDICTOR_CONFIG" << PREDCFG
DPPRED=$dppred
CBPRED=$cbpred
PHIST_THD=$phist_thd
BHIST_THD=$bhist_thd
PFQ_SIZE=$PFQ_SIZE
SHADOW_TABLE_SIZE=$SHADOW_TABLE_SIZE
PREDCFG

echo "Running experiment: $exp_name"
echo "DPPRED=$dppred CBPRED=$cbpred PHIST_THD=$phist_thd BHIST_THD=$bhist_thd"

# Run all benchmarks in parallel within this job
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

    echo "  Running \$benchmark..."
    $SNIPER -c run.cfg -i "\${BENCHMARK_INPUTS[\$benchmark]}" -p "\$benchmark" -n $CORES -d "\$output_dir" \
            > "\${output_dir}/stdout.log" 2> "\${output_dir}/stderr.log" &
done

wait
echo "Experiment $exp_name complete."
EOF

  chmod +x "$job_script"
  qsub "$job_script"
  echo "Submitted: $exp_name"
}

# Baseline: both predictors off
submit_job "baseline" 0 0 0 0

# dppred only: vary phist_thd from 0 to 6, cbpred=0
for thd in $(seq 0 6); do
  submit_job "pthd${thd}" 1 0 $thd 0
done

# cbpred with dppred: vary both thresholds
# Outer loop: phist_thd
# Inner loop: bhist_thd
for phist in $(seq 0 6); do
  for bhist in $(seq 0 6); do
    submit_job "pthd${phist}_bthd${bhist}" 1 1 $phist $bhist
  done
done
