#! /usr/bin/env python

import time, sys,os, subprocess as sb, pathlib
import argparse,tempfile

parser = argparse.ArgumentParser("Do a waterslide benchmark run for the npu2_demo.")
group = parser.add_mutually_exclusive_group()
group.add_argument("-p", "--parallel", action = 'store_false',default=True, help='use waterslide-parallel.')
group.add_argument("-s", "--serial", action = 'store_true',default=False, help='use waterslide.')
parser.add_argument('-m','--method',action='store',choices=['npu2','vnpu','vectornpu','vectormatchnpu','vectormatchre2','vectorre2'], default='vectormatchnpu')
parser.add_argument('-e','--expr',action='store',default='../bench_regexes.preproc/bench_regexes')
parser.add_argument('-t','--target',nargs='+',default=['../npu2_bench/packets.cut.wsproto','../npu2_bench/packest.cut_pt001.wsproto'])

args = parser.parse_args()
parallel = args.parallel or not args.serial
ws_exec  = './waterslide-parallel' if parallel else './waterslide'
pcap_in= [_ for _ in args.target if _.endswith('.pcap')]#pathlib.fnmatch.fnmatch(_, '**/*.pcap')]
wsproto_in= [_ for _ in args.target if _.endswith('.wsproto') or _.endswith('.mpproto')]#$pathlib.fnmatch.fnmatch(_, '**/*.{wsproto,mpproto}')]
with tempfile.NamedTemporaryFile() as script:
    script.write('%thread(1){')
    if pcap_in:
        script.write('pcap_in -r ' + ' -r '.join(pcap_in) + ' | bundle -N 256 -> $data_in\n')
    if wsproto_in:
        script.write('wsproto_in -r ' + ' -r '.join(wsproto_in) + ' | bundle -N 256 -> $data_in\n')

    if parallel:
        script .write( '}\n%thread(2){\n')
    script .write( ' $data_in | flush -N -t 1 -> $flushing; \n' +
                   ' $data_in | unbundle -> $unpacked; \n' +
                   ' $unpacked , $flushing | {method} CONTENT -F {expr} -M -L RESULT | bundle -N 256 -> $data_out\n'.format(method=args.method,expr = args.expr))
    if parallel:
        script.write('}\n%thread(3){\n')
    script.write(
        ' $data_out | unbundle -> $done\n' +
        ' $done | labelstat -> $print_out\n' +
        ' $done | bandwidth -> $print_out\n' +
        '}')
    script.flush()
#    with open(script.name,'rb') as tmp:
#        print(tmp.read())
    is_npu = args.method.endswith('npu')
    fmt_string = 'r:{}' if is_npu else 'l:{}'
    def ws_run():
        start = time.time()
        retval = sb.call([ws_exec ,'-F',script.name])
        end = time.time()
        print(fmt_string.format((end-start)))

    while True:
        ws_run()
