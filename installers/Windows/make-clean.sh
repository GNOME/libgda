#!/bin/bash

export CLEAN=yes
exec `dirname $0`/make-zip-setup.sh $1
