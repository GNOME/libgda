#!/bin/sh

for var in `find . -name "*.[ch]" -o -name "*.cpp"`
do
	sed -e 's/Gda_/Gda/g' < $var > $var.new
	mv -f $var.new $var
done
