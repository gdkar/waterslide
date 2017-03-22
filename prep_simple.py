#! /usr/bin/env python

import sys, subprocess as sb, re
import posix, posixpath as ppath
if len(sys.argv) < 2:
    print("usage: {} <infile> [ <outfile> ]".format(sys.argv[0]))
    sys.exit(-1)

odir = sys.argv[2] if (len(sys.argv) > 2) else (sys.argv[1] + '.preproc')
ofile = ppath.join(odir,sys.argv[1])
posix.mkdir(odir)
count = 0
expr = re.compile('"(.*)"(\\s*\\(.*?\\))?')
with open(sys.argv[1]) as _ifile:
    with open(ofile,'w') as _ofile:
        for expression in _ifile:
            try:
                expression = expression[:-1]
                print(expression)
                label = " (L{})".format(count)
                binfile = ppath.join(odir,'expression-{}.npup'.format(count))
                retcode = sb.call(['dc','-e',expression,'-o',binfile])
                if retcode < 0:
                    continue
                _ofile.write('"{}"\t{} "{}"\n'.format(expression,label, binfile));
                count += 1
                if retcode > 0:
                    print("non fatal error {}".format(retcode))
            except Exception as e:
                print(e)
