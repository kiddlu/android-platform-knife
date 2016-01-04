#!/bin/sh

THIS_PATH=$(dirname $0)
THIS_PATH=`cd $THIS_PATH;pwd`

export PATH=$THIS_PATH/output/bin:$PATH
