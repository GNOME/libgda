#!/usr/bin/env python3

import argparse
import os.path
import xml.etree.ElementTree as E

parser = argparse.ArgumentParser(description='Create a raw xml for providers specs.')
parser.add_argument('source', help='file path to create raw from')
parser.add_argument('-o', '--output', help='file output dir')

args=parser.parse_args()
dargs=vars(args)

spec = E.parse(dargs['source'])

def rename_attr (e, src, dest):
    val = e.get(src)
    if val is None:
        return
    e.set(dest, val)
    del e.attrib[src]

root = spec.getroot()

for c in root.iter():
    rename_attr(c, '_name', 'name')
    rename_attr(c, '_descr', 'descr')
    if c.tag.startswith('_'):
        c.tag = c.tag[1:]

n=os.path.basename(dargs['source'])
nfile=n.replace('.xml.in', '.raw.xml')
ofile=os.path.join(dargs['output'],nfile)
print (ofile)
spec.write(ofile)
