#! /usr/bin/env python
from __future__ import print_function

import struct, pathlib,random,argparse,sys,re, shlex

def check_size(pth):
    pth = pathlib.Path(pth).resolve()
    with pth.open('rb') as _file:
        bin_magic,bin_size = struct.unpack('ii',_file.read(8))
    if bin_magic != 0x339CC0DA:
        raise ValueError("invalid magic {}".format(hex(bin_magic)))
    return bin_size

def _check_size(pth):
    try:
        return check_size(pth)
    except:
        pass

def categorize(pth):
    pth = pathlib.Path(pth).resolve().absolute()
    res = dict()
    for f in pth.iterdir():
        sz = _check_size(f)
        if sz is not None:
            res[f] = sz

def pick(root, limit = 4255):
    root = pathlib.Path(root).resolve()

    count = 0
    total = 0
    expr = re.compile('(?P<first>["\'])(?P<expr>.*)(?P=first)\\s*(?P<label>\\(.*?\\))\\s*(?P<sec>[\'\"])(?P<path>.*)(?P=sec)')
    with root.open('rb') as _ifile:
        lines = [_.strip() for _ in _ifile.readlines()]
    random.shuffle(lines)
#    lines = sorted(lines,key=lambda x:len(shlex.split(x)[0]))
    for line in lines:
        try:
            parts = shlex.split(line)
            expr       = parts[0]
            binfile    = pathlib.Path(parts[2]).resolve().absolute()
            label      = parts[1]#gd['label']
            binsize    = check_size(binfile)
#            print(expr, binfile, label, binsize, file=sys.stderr)
            if binsize + total <= limit:
                print(line,file=sys.stdout)
                total += binsize
#                expression = expression[:-1]
            if total >= limit:
                break

        except Exception as e:
            print(e,file=sys.stderr)
#            pass

parser = argparse.ArgumentParser()
parser.add_argument('-e','--expr',type=pathlib.Path)
parser.add_argument('-n','--pes','--size',type=int,default=4255)

args = parser.parse_args()

pick(args.expr,limit=args.pes)
