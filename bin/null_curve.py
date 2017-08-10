#! /usr/bin/env python3

import sys, subprocess as sb, time

cmdline = [ './dummy-test','-e','expr','-N',sys.argv[2]]
start = time.time()
proc  = sb.Popen(cmdline,stdin=sb.PIPE,stdout=sb.PIPE,stderr=sb.PIPE)
proc.communicate(b'x' * int(sys.argv[1]))
proc.wait()
stop  = time.time()
print('{dsize},{size},{method},{expr},{time}'.format(dsize=sys.argv[2],size=sys.argv[1],method=sys.argv[0],expr='',time=stop-start))
