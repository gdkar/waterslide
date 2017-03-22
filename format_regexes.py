#! /usr/bin/env python

import sys, subprocess as sb, re
import posix, posixpath as ppath
if len(sys.argv) < 2:
    print("usage: {} <infile> [ <outfile> ]".format(sys.argv[0]))
    sys.exit(-1)

ofile = sys.argv[2] if (len(sys.argv) > 2) else (sys.argv[1] + '.preproc')
count = 0
expr = re.compile('"(.*)"(\\s*\\(.*?\\))?')
with open(sys.argv[1]) as _ifile:
    with open(ofile,'w') as _ofile:
        for expression in _ifile:
            try:
                if expression.endswith('\n'):
                    expression = expression[:-1]
                print(expression)
                label = " (R{})".format(count)
                _ofile.write('"{}"\t{}\n'.format(expression,label));
                count += 1
            except Exception as e:
                print(e)
