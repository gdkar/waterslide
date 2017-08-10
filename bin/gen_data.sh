#! /bin/sh
PREFIX=16MB_wsproto/
mkdir -p $PREFIX
for i in {10..2}
do
    for j in {7..4}
    do
        sz=$(( j << i ))
        ./waterslide "exec_in ./gen.py $sz $(( 1 << 24 )) 5 | wsproto_out -O $PREFIX/$sz"
    done
done
