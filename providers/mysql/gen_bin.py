#!/usr/bin/env python3

import argparse
import os

parser = argparse.ArgumentParser(description='Create a hander bin for providers.')
parser.add_argument('source', help='file path to create provider hander bin from')
parser.add_argument('-o', '--output', help='file output')

args=parser.parse_args()
dargs=vars(args)

f = open(dargs['source'])
s = str(f.read())
s = s.replace('SQLITE','MYSQL')
s = s.replace('sqlite','mysql')
s = s.replace('Sqlite','Mysql')
s = s.replace('SQLite','MySQL')
with open(os.path.join(dargs['output']), 'w') as file:
    file.write(s)



