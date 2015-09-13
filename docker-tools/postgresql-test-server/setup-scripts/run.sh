#!/bin/bash

__run_supervisor() {
supervisord -n -c /etc/supervisord.conf
}

# Call all functions
__run_supervisor
