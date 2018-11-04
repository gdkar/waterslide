#! /usr/bin/env python

import time, sys,os, subprocess as sb, pathlib
import argparse,tempfile

parser = argparse.ArgumentParser("Do a waterslide benchmark run for the npu2_demo.")
group = parser.add_mutually_exclusive_group()
group.add_argument("-p", "--parallel", action = 'store_false',default=True, help='use waterslide-parallel.')
group.add_argument("-s", "--serial", action = 'store_true',default=False, help='use waterslide.')
parser.add_argument('-m','--method',action='store', default='vectormatchnpu')
parser.add_argument('-e','--expr',action='store',default='../bench_set.txt.preproc/bench_set.txt')
parser.add_argument('--size',action='store',default='64',help='size of packet to run')
parser.add_argument('--count',action='store',default=2**24,help='approximate number of bytes to run')
parser.add_argument('--seed',action='store',default=5,help='random seed')
parser.add_argument('--once',action='store_true')
parser.add_argument('-T','--offset',default=None,type=int)

args = parser.parse_args()
parallel = args.parallel or not args.serial
ws_exec  = ['./waterslide-parallel'] if parallel else ['./waterslide']
if parallel and args.offset is not None:
    ws_exec += ['-W', '-T', '{}'.format(args.offset)]
method = args.method
if 'npu' in method:
    method += ' -m3 '

with tempfile.NamedTemporaryFile() as script:
    template = """
%thread(0){{
    exec_in ./gen.py {size} {count} {seed} | bundle -N {bundle} -> $in;
}}
%thread(2){{
    $in | unbundle | flush -t 5 | TAG:{method} -M -L MATCH -F {expr} | bundle -N {bundle} -> $out;
}}
%thread(4){{
    $out | unbundle | labelstat | print -VT -A /dev/null;
}}
"""
    script.write(
        template.format(
            size   = args.size,
            count  = args.count,
            seed   = args.seed,
            method = method,
            expr   = args.expr,
            bundle = 256
        )
    )
    script.flush()
    fmt_string = '{method}/{count}/{size}:{{}}'.format(
        method=args.method,
        count =args.count,
        size  =args.size
    )
    def ws_run():
        start = time.time()
        retval = sb.call(ws_exec + ['-F',script.name])
        end = time.time()
        print(fmt_string.format(end-start))

    while True:
        ws_run()
        if args.once:
            sys.exit(0)
