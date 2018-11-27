#! /usr/bin/env python2

import sys
import numpy
from numpy.random import bytes as rbytes,randint
lo = 128
hi = lo
if len(sys.argv) > 1:
    tmp = sys.argv[1]
    pre,_,post = tmp.partition('-')
    lo = int(pre)
    if post:
        hi = max(int(post),lo)

cnt = int(sys.argv[2]) if len(sys.argv) > 2 else 2**30
if len(sys.argv) > 3:
    numpy.random.seed(int(sys.argv[3]))

if hi <= lo:
    gen = lambda : rbytes(lo).replace('\n','\0') + '\n'
else:
    gen = lambda : rbytes(randint(lo,hi)).replace('\n','\0') + '\n'


for _ in range(cnt // ((lo + hi)//2)):
    sys.stdout.write(gen())
sys.stdout.flush()
