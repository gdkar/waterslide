#! /bin/sh

cd $(dirname $(realpath $0))
./bench.py --method vectormatchre2 --target  '../dummy.npup'  --expr ../bench.npup  --parallel --offset=0  2>/dev/null

