#! /usr/bin/env python

import sys, subprocess as sb, re
import posix, posixpath as ppath, pathlib
if len(sys.argv) < 2:
    print("usage: {} <infile> [ <outfile> ]".format(sys.argv[0]))
    sys.exit(-1)

odir = pathlib.Path(sys.argv[2] if (len(sys.argv) > 2) else (sys.argv[1] + '.preproc'))
ifile = pathlib.Path(sys.argv[1])
ofile = odir.joinpath(ifile.name)
od = ofile.parent
parts = []
while not od.exists():
    parts.append(od)
    od = od.parent
for od in parts:
    od.mkdir()
#od = pathlib.Path(odir.parts[0])
#for d in od.parts[1:]:
#    od = od.joinpath(d)
#    if not od.exists():
#        od.mkdir()
#dirs = []
#while not od.exists():
#    dirs.append(od.name)
#    od = od.parent
#for d in reversed(dirs):
#    od.with_name(d).mkdir()

count = 0
expr = re.compile('"(.*)"(\\s*\\(.*?\\))?')
with ifile.open('rb') as _ifile:
    with ofile.open('wb') as _ofile:
        for n,expression in enumerate(_ifile):
            try:
                expression = expression[:-1]
                label = " (L{})".format(count)
                binfile = odir.joinpath( 'expression-{}.npup'.format(count))
                retcode = sb.call(['dc','-qqqqqqqq','-s','16','-e',expression,'-o',binfile.as_posix()])
                if not binfile.exists():
                    print('FAILURE: {} "{}" returned {}'.format(n, expression, retcode))
                    continue
                _ofile.write('"{}"\t{} "{}"\n'.format(expression,label, binfile.as_posix()));
                count += 1
                if retcode > 0:
                    print("non fatal error {}".format(retcode))
            except Exception as e:
                print(e)
