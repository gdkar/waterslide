#! /bin/bash

cd $(dirname $(realpath $0))

(taskset -a 0x05 ./bench.py re2 2>/dev/null) &
RE2_PID=$!
(taskset -a 0x0a ./bench.py npu 2>/dev/null) &
NPU_PID=$!
trap 'kill -9 $NPU_PID $RE2_PID' EXIT
wait $RE2_PID $NPU_PID
