#!/bin/bash

# tar the php/ data
phpdir=../../providers/web/php
tar chf setup-data/php.tar $phpdir

# actual build
exec ../docker-tools.sh build Web
