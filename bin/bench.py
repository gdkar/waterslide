#! /usr/bin/env python

import time, sys,os, subprocess as sb

target = '/var/packets.mpproto'
if 'TARGET' in os.environ:
    target = os.environ['TARGET']

count = int(sys.argv[1])
method = sys.argv[2]

turbo = 1 if method == 're2' else 6
def run_npu():
    start = time.time()
    retval = sb.call(['./waterslide','wsproto_in -r {target} | calc "#PASS=(#COUNT++ < {count})" | npu CONTENT -F ../npu2_bench/ -H 386 -L RESULT'.format(target=target,count=count * turbo)])
    end = time.time()
    print('r:{}'.format((end-start)/turbo))

def run_vectormatchre2():
    start = time.time()
    retval = sb.call(['./waterslide','wsproto_in -r {target} | calc "#PASS=(#COUNT++ < {count})" | vectormatchre2 CONTENT -F ../npu2_bench/npu2_bench_set.npup -L RESULT -M'.format(target=target,count=count*turbo)])
    end = time.time()
    print('l:{}'.format((end-start)/turbo))

if method == 're2':
    while True:
        run_vectormatchre2()
else:
    while True:
        run_npu()
