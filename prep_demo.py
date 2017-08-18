#! /usr/bin/env python2

import sys, subprocess as sb, re
import posix, posixpath as ppath
import argparse,pathlib

parser = argparse.ArgumentParser('a ytility to precompile vectormatchnpu format config files for faster load')
parser.add_argument('input',action='store',help='input config file')
parser.add_argument('-o','--output',action='store',default=None,help='output directory')
parser.add_argument('-a','--absolute',action='store_true',help='use absolute paths')
parser.add_argument('--simple',action='store_true',help='use simple formating')
parser.add_argument('args',nargs=argparse.REMAINDER)

args = parser.parse_args()

odir = pathlib.Path(args.output or args.input + '.preproc')

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
                retcode = sb.call(['dc','-q','12','-e',expression,'-o',binfile.as_posix()] + args.args)
                if not binfile.exists():
                    print('FAILURE: {} "{}" returned {}'.format(retcode, expression, retcode))
                    continue

                _ofile.write('"{}"\t{} "{}"\n'.format(expression,label, binfile.as_posix()));
                count += 1
            except Exception as e:
                    print(e)
