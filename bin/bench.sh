#! /bin/bash

cd $(dirname $(realpath $0))

TARGET="$1"
shift

(./bench.py -m vectormatchre2 --target="$TARGET" 2>/dev/null) &
RE2_PID=$!
(./bench.py -m vectormatchnpu --target="$TARGET" 2>/dev/null) &
NPU_PID=$!
trap 'kill -9 $NPU_PID $RE2_PID' EXIT
wait $RE2_PID $NPU_PID
