#!/bin/bash

/update-cfg.sh
/usr/sbin/php-fpm -D
/usr/sbin/nginx
