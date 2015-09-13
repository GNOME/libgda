#!/bin/bash

conf="/usr/share/nginx/html/php/gda-secure-config.php"
hostip=`route -n | grep UG | awk '{print $2}'`

cat ${conf}.tmpl | sed -e "s/HOSTIP/$hostip/g" > $conf
