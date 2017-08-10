#! /bin/sh

cd $(dirname $(realpath $0))
./bench.py --method vectormatchnpu --target  '../packets.wsproto'  --expr dummy.npup  --parallel --offset=0 

