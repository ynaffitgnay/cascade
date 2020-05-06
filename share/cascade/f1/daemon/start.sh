#!/bin/bash

{
export HOME_DIR=/home/centos/src/project_data
export AWS_FPGA=$HOME_DIR/aws-fpga
export BUILD_DIR=$(dirname $(readlink -f $0))

# Setup SDK env
source $AWS_FPGA/sdk_setup.sh >/dev/null 2>&1
rm -f awsver.txt

cd $BUILD_DIR
make
RC=$?

if [ $RC -ne 0 ]; then
	echo "Failed to compile AOS daemon"
	exit 1
fi

sudo rm -f /tmp/aos_daemon.socket
sudo ./daemon >/dev/null 2>&1

exit 0
}
