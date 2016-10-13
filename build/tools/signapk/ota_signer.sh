#!/bin/sh
# sign OTA zip to update.zip

version="ota_signer 1.0
Written by Kidd Lu."

usage="Usage: ota_signer key_dir input.zip output.zip ... ..."

case $1 in
--help)    exec echo "$usage";;
--version) exec echo "$version";;
esac

if [ ! -n "$1" ]; then 
  echo -e "\033[31mErr: Key Dir does not exist\033[0m"
  echo ""
  exec echo "$usage"
  exit 1 
fi

if [  ! -n "$2" ]; then 
  echo -e "\033[31mErr: INPUT does not exist\033[0m"
  echo ""
  exec echo "$usage"
  exit 1 
fi

if [  ! -n "$3" ]; then 
  echo -e "\033[31mErr: OUTPUT does not exist\033[0m"
  echo ""
  exec echo "$usage"
  exit 1 
fi

SCRIPT_DIR=`dirname "$0"`
KEY_DIR=$1
INPUT=$2
OUTPUT=$3

#java -jar "$SCRIPT_DIR"/signapk.jar -w "$SCRIPT_DIR"/../../target/product/security/testkey.x509.pem "$SCRIPT_DIR"/../../target/product/security/testkey.pk8 $@
#java -jar "$SCRIPT_DIR"/signapk.jar -w $KEY_DIR/releasekey.x509.pem $KEY_DIR/releasekey.pk8 $INPUT $OUTPUT
