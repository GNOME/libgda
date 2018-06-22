#!/usr/bin/python

import argparse
import os.path
import xml.etree.ElementTree as E

parser = argparse.ArgumentParser(description='Create a raw xml for providers specs.')
parser.add_argument('source', help='file path to create raw from')
parser.add_argument('-o', '--output', help='file output dir')

parser.parse_args()

spec = E.parse(source)

def rename_attr (e, src, dest):
    val = e.get(src)
    e.set(dest, val)
    e.set(src, None)

def rename_element_attr(e, src, dst):
    rename_attr(e,src,dst)
    for c in e.iter():
        rename_name_attr(c,src,dst)

root = spec.getroot()
rename_element_attr(root, '_name', 'name')
rename_element_attr(root, '_descr', 'descr')

n=path.basename(source)
nfile = n.replace('.xml.in', '.xml')
spec.write(path.join(output,nfile))
