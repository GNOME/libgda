#!/bin/sh

if [ -n "${PREFIX+x}" ]
then
    echo "The PREFIX environment variable is not set, using /local"
    export PREFIX=/local
fi

if ! [ -f gda-browser.bundle ]
then
    echo "This script must be executed in the directory which contains the gda-browser.bundle file"
    exit 1
fi

ige-mac-bundler gda-browser.bundle
