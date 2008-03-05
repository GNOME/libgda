#!/bin/sh

#
# This script prepares the sources to write a new database provider (it renames the files and
# the objects within the files) using a new name given as the only argument of the command
# line.
#
# Usage:
# 1 - Copy the ./skel-implementation/capi or ./skel-implementation/models to another directory (for example MySQL)
# 2 - go into the new directory (MySQL/ in this case)
# 3 - execute this script with the name of the provider to create (mysql in this case)
#     ../prepare_provider_sources.sh mysql
# 

if [ $# != 1 ]
then
  echo "Usage: `basename $0` <provider name>"
  exit 1
fi  
provname=$1

#
# compute what to rename from ("capi", "models")
#
for file in $( ls | grep "gda-[a-z]*.h"); 
do
    base=$(echo $file | cut -b 5- | sed -e 's/\.h$//')
done

#
# renaming files
#
for file in *
do
    newname=$(echo $file | sed -e "s/$base/$provname/")
    if [ $file != $newname ]; then
	mv $file $newname
    fi
done

#
# changing contents
#
upname=$(echo $provname | cut -b 1 | tr a-z A-Z)$(echo $provname | sed -e "s/^.//")
allupname=$(echo $provname | tr a-z A-Z)
bupname=$(echo $base | cut -b 1 | tr a-z A-Z)$(echo $base | sed -e "s/^.//")
ballupname=$(echo $base | tr a-z A-Z)


for file in *
do
    mv $file $file.1
    cat $file.1 | sed -e "s/$base/$provname/g" -e "s/$bupname/$upname/g" -e "s/$ballupname/$allupname/g"> $file
    rm -f $file.1
done
