#! /usr/bin/env python

import sys, subprocess as sb, re
import posix, posixpath as ppath
import argparse

parser = argparse.ArgumentParser('a ytility to precompile vectormatchnpu format config files for faster load')
parser.add_argument('-i','--input',action='store',help='input config file')
parser.add_argument('-o','--output',action='store',default=None,help='output directory')
parser.add_argument('args',nargs=argparse.REMAINDER)

args = parser.parse_args()

odir = args.output or args.input + '.preproc'

ofile = ppath.join(odir,ppath.basename(args.input))

posix.mkdir(odir)
count = 0
expr = re.compile('"(.*)"(\\s*\\(.*?\\))?')
with open(sys.argv[1]) as _ifile:
    with open(ofile,'w') as _ofile:
        for line in _ifile:
            try:
                m = expr.match(line)
                expression = m.group(1).replace('/','\\x2f')
                print(expression)
                label = m.group(2) or " (L{})".format(count)
                binfile = ppath.join(odir,'{}-{}'.format(count,expression))
                sb.check_call(['dc','-s','16','-e',expression,'-o',binfile] + args.args)
                _ofile.write('"{}"\t{} "{}"\n'.format(expression,label, binfile));
                count += 1
            except Exception as e:
                print(e)
