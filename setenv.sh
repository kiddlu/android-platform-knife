#!/bin/sh

THIS_PATH=`readlink -f $0 | xargs dirname`

export PATH=$THIS_PATH/output/bin:$PATH
