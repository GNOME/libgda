#!/bin/bash
#
# Main docker script
#

function HELP {
	echo "Usage: $0 <command> [options]"
	echo "   commands:"
	echo "       build:   creates a docker image for the provider specified as option"
	echo "                (MUST be called from the docker image's specific directory)"
	echo "       start:   starts a docker container for the provider specified as option"
	echo "       stop:    stops the docker container for the provider specified as option"
	echo "       info:    get information about the container running the provider specified as option"
	echo "       started: tests if the docker container for the provider specified as option is started"
	echo "                returns: 0 is started, and 1 otherwise"
	echo "   ex: $0 start MySQL"
	exit 1
}

function ensure_image_exists {
    image_name=$1
    echo "Using image: $image_name"
    img=`${docker_cmd} images -q "$image_name"`
    if test "x$img" == "x"
    then
	echo "Docker image not found, use the ./build.sh script first to create it"
	exit 1
    fi
}

function remove_image_if_exists {
    image_name=$1
    echo "Using image: $image_name"
    img=`${docker_cmd} images -q "$image_name"`
    if test "x$img" != "x"
    then
	echo "Docker image found, removing it..."
	$docker_cmd rmi "$image_name" || {
	    echo "Failed to remove image."
	    exit 1
	}
    fi
}

function ensure_container_running {
    cont_name=$1
    ${docker_cmd} inspect "$cont_name" > /dev/null 2>&1 || {
	echo "Docker container '$cont_name' is not running"
	exit 1
    }
}

function ensure_container_not_running {
    cont_name=$1
    ${docker_cmd} inspect "$cont_name" > /dev/null 2>&1 && {
	echo "Docker container '$cont_name' is running, you need to stop it first"
	exit 1
    }
}

docker_cmd=`which docker` || {
    echo "Can't find docker, make sure it is installed, refer to https://www.docker.com/"
    exit 1
}

"$docker_cmd" version > /dev/null 2>&1 || {
    docker_cmd="sudo $docker_cmd"
    $docker_cmd version > /dev/null 2>&1 || {
	echo "Can't execute docker"
	exit 1
    }
}
echo "Using Docker command: $docker_cmd"

function parse_provider_arg {
    provider_name=$1
    if test "x$provider_name" == "x"
    then
	HELP
    fi
    prov=${provider_name,,}
    case $prov in
	mingw*)
	    image_name="libgda-$prov"
	    container_name="cx-$prov"
	    ;;
	*)
	    image_name="libgda-test-$prov"
	    container_name="test-$prov"
	    ;;
    esac
}

function get_port_for_provider {
    provider_name=${1,,}
    case $provider_name in
	mysql)
	    echo 3306
	    ;;
	postgresql)
	    echo 5432
	    ;;
	ldap)
	    echo 389
	    ;;
	web)
	    echo 80
	    ;;
	oracle)
	    echo 1521
	    ;;
	firebird)
	    echo 3050
	    ;;
	mingw*)
	    echo mingw
	    ;;
	*)
	    echo "Unknown database server, could no determine port number"
	    exit 1
	    ;;
    esac
}

case $1 in
    build)
	parse_provider_arg $2
	ensure_container_not_running "$container_name"
	remove_image_if_exists "$image_name"
	echo "Now building Docker image, this will take a few minutes (or maybe half an hour, depending on you setup)..."
	$docker_cmd build --force-rm -q -t "$image_name" . || {
	    echo "Failed to build image."
	    exit 1
	}
	echo "Image '$image_name' is now ready, you can use the start.sh script"
	;;
    start)
	parse_provider_arg $2
	ensure_image_exists "$image_name"
	ensure_container_not_running "$container_name"
	port=`get_port_for_provider $2`
	if test "x$port" == "xmingw"
	then
	    # MinGW session
	    echo "Running $provider_name, use CTRL-D to stop it"
	    $docker_cmd run -ti -v ${gda_src}:/src/libgda:ro -v ${destdir}:/install -e UID=`id -u` -e GID=`id -g` --rm "$image_name" || {
		echo "Failed"
		exit 1
	    }
	else
	    # background
	    echo "Running $provider_name server in the background, listening on port $port use 'stop.sh' to stop it"
	    $docker_cmd run -d -p $port:$port --name "$container_name" "$image_name" > /dev/null 2>&1 || {
		echo "Failed"
		exit 1
	    }
	    ipaddr=`$docker_cmd inspect --format='{{.NetworkSettings.IPAddress}}' "$container_name"`
	    echo "IP address of container is $ipaddr"
	fi
	;;
    info)
	parse_provider_arg $2
	ensure_image_exists "$image_name"
	ensure_container_running "$container_name"
	ipaddr=`$docker_cmd inspect --format='{{.NetworkSettings.IPAddress}}' "$container_name"`
	echo "IP address of container is $ipaddr"
	;;
    stop)
	parse_provider_arg $2
	ensure_container_running "$container_name"
	echo "Stopping $provider_name server..."
	$docker_cmd kill "$container_name" > /dev/null 2>& 1 || {
	    echo "Failed to kill container"
	    exit 1
	}
	$docker_cmd rm "$container_name" > /dev/null 2>& 1 || {
	    echo "Failed to remove container"
	    exit 1
	}
	;;
    started)
	parse_provider_arg $2
	${docker_cmd} inspect "$container_name" > /dev/null 2>&1 || {
	    echo "stopped"
	    exit 1
	}
	echo "started"
	exit 0
	;;
    *)
	HELP
	;;
esac
