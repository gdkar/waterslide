#! /usr/bin/env python

import time, sys,os, subprocess as sb

if 'TARGET' in os.environ:
    target = os.environ['TARGET']

method = sys.argv[1]

target = ' -r ../npu2_bench/packets.cut.wsproto  -r ../npu2_bench/packets.cut_pt001.wsproto  '

if method == 're2':
    turbo = 1
else:
    turbo = 6

target = target * turbo

#targets = ['../npu2_bench/packets.cut.wsproto'] + [ '../npu2_bench/packets.cut_pt{:03}.wsproto'.format(x) for x in range(1,turbo)]
#target = ' -r '.join(targets)
#target = ' -r '.join('../npu2_bench/packets.cut_pt{x:03}.wsproto'.format(x=x) for x in range(1,turbo+1))
#target_turbo = '../npu2_bench/packets.cut_pt00{{1..{turbo}}}.wsproto'.format(turbo=turbo)
def run_npu():
    start = time.time()
    retval = sb.call(['./waterslide','wsproto_in {target} | npu CONTENT -F ../npu2_bench/ -H 585 -L RESULT'.format(target=target)])
    end = time.time()
    print('r:{}'.format((end-start)/turbo))

def run_vectormatchre2():
    start = time.time()
    retval = sb.call(['./waterslide','wsproto_in {target} | vectormatchre2 CONTENT -F ../npu2_bench_set.npup -L RESULT -M'.format(target=target)])
    end = time.time()
    print('l:{}'.format((end-start)/turbo))

function = run_vectormatchre2 if method == 're2' else run_npu
while True:
    function()
