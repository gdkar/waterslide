#! /usr/bin/env python3
from __future__ import print_function, division

import sys
import os
import numpy
from numpy.random import bytes as rbytes,normal
import argparse, string, base64

print(sys.argv)
def hexbytes(i):
    return '{:02X}'.format(i%255).encode('latin1')

def to_hex(b):
    return b''.join(hexbytes(_) for _ in b)

_hexdigits_fs = frozenset(map(ord,string.hexdigits)) | frozenset(b'\n')

def to_asciihex(b):
    agg = []
    i = 0
    while i < len(b):
        t = b[i:i+2]
        if any(_ in _hexdigits_fs for _ in t):
            agg.extend(to_hex(_) for _ in t)
        else:
            agg.extend(t)
        i += 2
    return b''.join(agg)


encoders = {
    'none':lambda b:b.replace(b'\n',b''),
    'base64':base64.encodebytes,
    'hex':to_hex,
    'asciihex':to_asciihex,
}
parser = argparse.ArgumentParser('generate random stuffffffff')
parser.add_argument('-n',dest='count',action='store',type=int,default=(1<<16))
parser.add_argument('-m',dest='mean',action='store',type=float,default=2048.)
parser.add_argument('-s',dest='std',action='store',type=float,default=256.)
parser.add_argument('-e',dest='encode',action='store',choices=['base64','hex','asciihex','none'],default='base64')
parser.add_argument('-S',dest='seed',action='store',type=int,default=None)

args = parser.parse_args()
encoder = encoders.get(args.encode,encoders.get('base64'))

loc   = args.mean
scale = args.std
count = args.count

if loc < 1 or scale <= 0:
    raise ValueError('invalid normal distribution parameters')

if count <= 1:
    raise ValueError('invalid count')

if args.seed is not None:
    numpy.random.seed(args.seed % (2**32))

if scale <= 1:
    loc = int(loc)
    gen = lambda : encoder(rbytes(loc)) + b'\n'
else:
    gen = lambda : encoder(rbytes(max(16,int(normal(loc=loc,scale=scale,size=None))))) + b'\n'

unbuffered = os.fdopen(sys.stdout.fileno(), 'wb', 0)
for _ in range(count):
    d = gen()
    unbuffered.write(d)
    unbuffered.flush()

unbuffered.flush()
