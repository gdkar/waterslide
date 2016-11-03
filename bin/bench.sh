#! /bin/bash

cd $(dirname $(realpath $0))

./bench.py $(( 1 << 19 )) re2 2>/dev/null &
RE2_PID=$!
./bench.py $(( 1 << 19 )) npu 2>/dev/null &
NPU_PID=$!
trap 'kill -9 $NPU_PID $RE2_PID' EXIT
wait $RE2_PID $NPU_PID
