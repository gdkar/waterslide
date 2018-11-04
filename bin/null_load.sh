#! /bin/sh

cd $(dirname $(realpath $0))
./bench.py --method vectormatchre2 --target  '../packets.wsproto'  --expr ./dummy.npup  --parallel --offset=2 2>/dev/null
