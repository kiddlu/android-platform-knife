#! /bin/sh


SCRIPT_DIR=`dirname "$0"`
SCRIPT_DIR=`dirname "$SCRIPT_DIR"`

java -Xmx512M -jar "$SCRIPT_DIR"/BootSignature.jar "$@"