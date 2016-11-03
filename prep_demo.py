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
        for line in _ifile:
            try:
                m = expr.match(line)
                expression = m.group(1).replace('/','\\x2f')
                print(expression)
                label = m.group(2) or " (L{})".format(count)
                sb.check_call(['dc','-e',expression,'-o',ppath.join(odir,expression)])
                _ofile.write('"{}"\t{}\n'.format(expression,label))
                count += 1
            except Exception as e:
                print(e)
