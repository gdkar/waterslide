#! /usr/bin/env python2
from __future__ import print_function

import sys, subprocess as sb, re
import posix, posixpath as ppath
import argparse,pathlib, resource, signal


parser = argparse.ArgumentParser('a ytility to precompile vectormatchnpu format config files for faster load')
parser.add_argument('input',action='store',help='input config file')
parser.add_argument('-o','--output',action='store',default=None,help='output directory')
parser.add_argument('-a','--absolute',action='store_true',help='use absolute paths')
parser.add_argument('-j','--jobs',action='store',type=int,default=24,help='number of concurrent jobs to leave outstanding')
group = parser.add_mutually_exclusive_group()
group.add_argument('--simple',action='store_true',help='use simple formatting')
group.add_argument('-f','--fmt','--format',action='store_true',help='only reformat simple -> vnpu')
#parser.add_argument('--simple',action='store_true',help='use simple formating')
parser.add_argument('args',nargs=argparse.REMAINDER)

args = parser.parse_args()

odir = pathlib.Path(args.output or args.input + '.preproc')

def limit_Popen(time_limit=15, mem_limit=int(2**31),input=None, cmdline=[],*args, **kwargs):
    def preexec():
        if mem_limit:
            as_rlimit = mem_limit//1024
            resource.setrlimit(resource.RLIMIT_RSS,(as_rlimit,as_rlimit))

        signal.alarm(int(time_limit))
#    try:
    return sb.Popen(cmdline,*args,preexec_fn=preexec, **kwargs)

if args.absolute:
    odir = odir.resolve().absolute()

ifile = pathlib.Path(args.input)
ofile = odir.joinpath(ifile.name)
od = ofile.parent
parts = []
while not od.exists():
    parts.append(od)
    od = od.parent
for od in parts:
    od.mkdir()

#posix.mkdir(odir)
count = 0
expr = re.compile('"(.*)"(\\s*\\(.*?\\))?')
with ifile.open('rb') as _ifile:
    with ofile.open('wb') as _ofile:
        q = []
        if args.fmt:
            for line in _ifile:
                expression = line[:-1]
                label = " (L{})".format(count)
                _ofile.write('"{}"\t{}\n'.format(expression,label));
                count += 1
        else:
            def proc(wait=args.jobs):
                while q:
                    pop = q[0][0]
                    retcode = pop.wait() if (len(q) > wait) else pop.poll()
                    if retcode is not None:
                        pop,expression,label,binfile = q.pop(0)
                        retcode = pop.wait()
                        if retcode < 0:
                            print('warning, expression {} returned error code {}'.format(expression,retcode))
                        if not binfile.exists():
                            print('FAILURE: {} "{}" returned {}'.format(retcode, expression, retcode))
                            continue

                        _ofile.write('"{}"\t{} "{}"\n'.format(expression,label, binfile.as_posix()))
                    elif len(q) <= wait:
                        break

            for line in _ifile:
                try:
                    if args.simple:
                        expression = line[:-1]
                        label = " (L{})".format(count)
                    else:
                        m = expr.match(line)
                        expression = m.group(1).replace('/','\\x2f')
                        label = m.group(2) or " (L{})".format(count)

                    binfile = odir.joinpath( 'expression-{}.npup'.format(count))
                    pop = limit_Popen(cmdline=['dc','-q','12','-e',expression,'-o',binfile.as_posix()] + args.args)
                    q.append((pop,expression,label,binfile))
                    count += 1
                    proc()
                except Exception as e:
                    print(e)
            proc(0)
