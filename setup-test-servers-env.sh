#!/bin/bash

#
# This script starts all the test servers which are present as Docker images (see
# docker-tools/ directory for more details), sets up some environment variables, and
# starts a sub shell.
#
# When the sub shell exits, all the docker containers which have been started are stopped.
#

dir=`dirname $0`
envfile=test-servers-env

function control_servers {
    op=$1
    case $op in
	start)
	    ;;
	stop)
	    ;;
	*)
	    echo "Unknown operation."
	    exit 1
	    ;;
    esac

    for srv in Firebird Ldap MySQL Oracle PostgreSQL Web
    do
	echo -n "$srv..."
	$dir/docker-tools/docker-tools.sh $op $srv > /dev/null 2>&1
	if [ $? != 0 ]
	then
	    echo "Failure!"
	else
	    echo "Ok!"
	    uppername=${srv^^}
	    if [ $srv == "Oracle" ]
	    then
		dbname="xe"
	    else
		dbname="gda"
		if [ $op == "start" ]
		then
		    echo "export ${uppername}_DBCREATE_PARAMS=\"HOST=localhost;ADM_LOGIN=gdauser;ADM_PASSWORD=gdauser\"" >> $envfile
		else
		    echo "export -n ${uppername}_DBCREATE_PARAMS=" >> $envfile
		fi
	    fi

	    if [ $op == "start" ]
	    then
		echo "export ${uppername}_CNC_PARAMS=\"HOST=localhost;USERNAME=gdauser;PASSWORD=gdauser;DB_NAME=$dbname\"" >> $envfile
	    else
		echo "export -n ${uppername}_CNC_PARAMS=" >> $envfile
	    fi
	fi
    done
}

echo "Starting tests servers..."
rm -f $envfile
control_servers start
. $envfile
echo "Now entering sub shell. Closing this sub shell will stop all the test servers."
echo "Environment variables are in the '$envfile' file"
/bin/bash
echo "Stopping tests servers..."
rm -f $envfile
control_servers stop
. $envfile
rm -f $envfile



