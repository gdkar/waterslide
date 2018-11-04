#! /bin/bash
PREFIX="16MB_wsproto/"
SUFFIX=".wsproto"
MULTI=256
SINGLE=$(( 1 << 24 ))
BYTES=$(( SINGLE * MULTI ))
SEED=5
rm -r $PREFIX
mkdir -p $PREFIX
for i in {10..2}
do
    for j in {7..4}
    do
        SIZE=$(( j << i ))
        ./waterslide "exec_in ./gen.py $SIZE $SINGLE $SEED | wsproto_out -O $PREFIX/$SIZE" >/dev/null 2>&1
    done
done
echo data_size, packet_size, method, expressions, time
for i in $PREFIX*$SUFFIX
do
    SIZE="${i#$PREFIX}"
    SIZE="${SIZE%$SUFFIX}"
    for j in {0..1}
    do 
        printf "${BYTES}, ${SIZE},"
        ./curve.py --method vectormatchre2 --target "$i" --multi $MULTI --once --expr ./dummy.npup 2>/dev/null
    done
    for j in {0..1}
    do 
        printf "${BYTES}, ${SIZE},"
        ./curve.py --method vectormatchnpu --target "$i" --multi $MULTI --once --expr ../bench.npup 2>/dev/null
    done
    for j in {0..1}
    do
        ./null_curve.py $SIZE $BYTES
    done
done
