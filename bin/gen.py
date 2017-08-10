#! /usr/bin/env python2

import sys
from numpy.random import bytes as rbytes, seed as nseed
from random import choice, seed as rseed
val = int(sys.argv[1]) if len(sys.argv) > 1 else 128
cnt = int(sys.argv[2]) if len(sys.argv) > 2 else 2**30
seed= int(sys.argv[3]) if len(sys.argv) > 3 else 5

rseed(seed)
nseed(seed)

pkts = cnt // val
left = cnt - ( val * pkts)

gen = lambda v:(rbytes(v).replace('\n','\0') + '\n') 

if pkts <= 16:
    for _ in range(pkts):
        sys.stdout.write(gen(val))
else:
    tmp = [gen(val) for _ in range(16)]
    for _ in range(pkts):
        sys.stdout.write(choice(tmp))

if left:
    sys.stdout.write(gen(left))

sys.stdout.flush()
