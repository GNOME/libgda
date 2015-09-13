#!/bin/bash

# build image
echo "Generating LDAP directory contents"
pushd setup-data > /dev/null 2>&1 && ./gen_names_ldif.pl && popd > /dev/null 2>&1 || {
    echo "Error"
    exit 1
}

# actual build
exec ../docker-tools.sh build LDAP
