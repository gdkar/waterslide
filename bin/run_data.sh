#! /bin/bash
PREFIX="16MB_wsproto/"
SUFFIX=".wsproto"
MULTI=4
BYTES=$(( ( 1 << 24 ) * MULTI ))
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
