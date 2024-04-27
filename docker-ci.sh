#!/bin/bash

# use this script to build and test the xv6 image in a docker container.
# if <ubuntu-version> is not specified, it defaults to 22.04.
# Run this script with: ./docker-ci.sh ...
#                                      build       <ubuntu-version>
#                                      test        <ubuntu-version>
#                                      interactive <ubuntu-version> <command>

LINTING_FLAG=false # Default value

# Check for the --lint flag
for arg in "$@"; do
    if [ "$arg" == "--lint" ]; then
        LINTING_FLAG=true
        break
    fi
done

# Set the default version of Ubuntu to use
if [ -z "$2" ]; then
    UBUNTU_VERSION="22.04"
elif [ "$2" == "16.04" ]; then
    UBUNTU_VERSION="16.04"
elif [ "$2" == "22.04" ]; then
    UBUNTU_VERSION="22.04"
else
    echo "Invalid Ubuntu version: $2"
    exit 1
fi

# Set the Dockerfile based on the Ubuntu version
if [ "$UBUNTU_VERSION" == "16.04" ]; then
    DOCKERFILE="Dockerfile_16.04.dockerfile"
    IMAGE_NAME="xv6-test-image-16-04"
else
    DOCKERFILE="Dockerfile"
    IMAGE_NAME="xv6-test-image"
fi


if [ "$1" == "build" ]; then
    echo "Building Docker image $IMAGE_NAME"
    docker build --build-arg USERNAME=$(whoami) \
                --build-arg GRPNAME=$(id -gn) \
                --build-arg UID=$(id -u) \
                --build-arg BUILD_LINTING_TOOLS=$LINTING_FLAG \
                --build-arg GID=$(id -g) -t $IMAGE_NAME -f $DOCKERFILE .
    exit 0
fi


#########################################################################
# test the docker image using the run-ci.sh script or intaractive mode

# Check the first argument to determine what to do
if [ "$1" == "test" ]; then
    #### Generate OCI images locally
    make build_oci
    # Run tests, dind required for building test oci images!
    docker run --mount type=bind,source="$(pwd)",target=/home/$(whoami)/xv6 \
                --rm --privileged -u 0 $IMAGE_NAME \
                /home/$(whoami)/xv6/run-ci.sh
elif [ "$1" == "interactive" ]; then
    # Run interactive command
    if [ -z "$3" ]; then
        echo "Usage: $0 interactive <ubuntu-version> <command>"
        exit 1
    fi
    docker run -it \
        --mount type=bind,source="$(pwd)",target=/home/$(whoami)/xv6 \
        --rm --privileged -u 0 $IMAGE_NAME $3
else
    echo "Invalid command: $1"
    exit 1
fi
