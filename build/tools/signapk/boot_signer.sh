#! /bin/sh

#verity key dir build\target\product\security
#java -Xmx512M -jar BootSignature.jar /boot boot.img z:\work\t3\build\target\product\security\verity.pk8 z:\work\t3\build\target\product\security\verity.x509.pem boot.img.verify

SCRIPT_DIR=`dirname "$0"`
#SCRIPT_DIR=`dirname "$SCRIPT_DIR"`

java -Xmx512M -jar "$SCRIPT_DIR"/BootSignature.jar "$@"