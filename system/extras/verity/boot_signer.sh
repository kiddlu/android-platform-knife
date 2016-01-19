#!/bin/sh

# Start-up script for BootSigner

BOOTSIGNER_HOME=`dirname "$0"`

java -Xmx512M -jar $BOOTSIGNER_HOME/BootSignature.jar /boot $1 $BOOTSIGNER_HOME/../../security/verity.pk8 $BOOTSIGNER_HOME/../../security/verity.x509.pem $1-sign
