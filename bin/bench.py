#! /usr/bin/env python

from __future__ import print_function
import time, sys,os, subprocess as sb, pathlib
import argparse,tempfile

parser = argparse.ArgumentParser("Do a waterslide benchmark run for the npu2_demo.")

group = parser.add_mutually_exclusive_group()
group.add_argument("-p", "--parallel", action = 'store_false',default=True, help='use waterslide-parallel.')
group.add_argument("-s", "--serial", action = 'store_true',default=False, help='use waterslide.')

group = parser.add_mutually_exclusive_group()
group.add_argument('--json',action='store',type=pathlib.Path,help='location to dump results in json format',default=None)
group.add_argument('--print',action='store',type=pathlib.Path,help='location to dump results in default format',default=None)
group.add_argument('--csv',action='store',type=pathlib.Path,help='location to dump results in csv format',default=None)
parser.add_argument('--dump_args',nargs='+',default=[],help='extra dump output arguments')


parser.add_argument('-m','--method',action='store', default='vectormatchnpu')
parser.add_argument('-e','--expr',action='store',default='../bench_set.txt.preproc/bench_set.txt')
parser.add_argument('-t','--target',nargs='+',default=['../npu2_bench/packets.cut.wsproto','../npu2_bench/packest.cut_pt001.wsproto'])
parser.add_argument('-T','--offset',default=None,type=int)
parser.add_argument('--once',action='store_true')
parser.add_argument('--multi',action='store',default=1,type=int)

args = parser.parse_args()
parallel = args.parallel or not args.serial
ws_exec  = ['./waterslide-parallel'] if parallel else ['./waterslide']
if parallel and args.offset is not None:
    ws_exec += ['-W', '-T', '{}'.format(args.offset)]
pcap_in= args.multi * [_ for _ in args.target if _.endswith('.pcap')]#pathlib.fnmatch.fnmatch(_, '**/*.pcap')]
wsproto_in= args.multi * [_ for _ in args.target if _.endswith('.wsproto') or _.endswith('.mpproto')]#$pathlib.fnmatch.fnmatch(_, '**/*.{wsproto,mpproto}')]
if 'npu' in args.method:
    args.method += ' -m3 '

with tempfile.NamedTemporaryFile() as script:
    script.write('%thread(0){')
    if pcap_in:
        script.write('pcap_in -r ' + ' -r '.join(pcap_in) + ' | bundle -N 256 -> $data_in\n')

    if wsproto_in:
        script.write('wsproto_in -r ' + ' -r '.join(wsproto_in) + ' | bundle -N 1024 -> $data_in\n')

    if parallel:
        script .write( '}\n%thread(2){\n')
    script .write( ' $data_in | flush -N -t 16 -> $flushing; \n' +
                   ' $data_in | unbundle -> $unpacked; \n' +
                   ' $unpacked , $flushing | {method} -F {expr} -M -L RESULT | bundle -N 1024 -> $data_out\n'.format(method=args.method,expr = args.expr))
    if parallel:
        script.write('}\n%thread(4){\n')
    script.write(
        ' $data_out | unbundle -> $done\n' +
        ' $done | labelstat -> $print_out\n')
    if args.csv:
        script.write(' $done | print -s \',\' {} -A {}\n'.format(' '.join(args.dump_args),args.csv.as_posix()
        ))
    if args.json:
        script.write(' $done | print2json -O {}\n'.format(args.json.as_posix()))
    if args.print:
        script.write(' $done | print {} -A {}\n'.format(' '.join(args.dump_args),args.print.as_posix()))
        
    script.write(
        ' $done | bandwidth -> $print_out\n' +
        ' $print_out | print  -VVT -A /dev/null\n' +
#        ' $print_out | print\n' +
        '}')
    script.flush()
#    with open(script.name,'rb') as tmp:
#        print(tmp.read())
    is_npu = args.method.endswith('npu')
    fmt_string = 'r:{}' if is_npu else 'l:{}'
    def ws_run():
        start = time.time()
        retval = sb.call(ws_exec + ['-F',script.name])
        end = time.time()
        print(fmt_string.format((end-start)))

    while True:
        ws_run()
        if args.once:
            sys.exit(0)
