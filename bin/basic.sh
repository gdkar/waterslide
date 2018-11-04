#! /bin/bash
cat ../data.basic.csv | ./waterslide-parallel "csv_in JSON  -d '\t' -i  | json JSON | TAG:vnpu data -L MATCH  -M -F ../exprs.basic.preproc/exprs.basic -v9 -m9 -D '/sys/bus/pci/devices/0000:07:00.0'  | mklabelset MATCH -L TAGS -S -I data JSON  | print2json"

