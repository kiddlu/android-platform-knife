#!/bin/sh
# Unpack ramdisk.img

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

version="unpackramdisk 1.0
Written by Kidd Lu."

usage="Usage: unpackramdisk file [output dir]... ...

if no output dir, use default ./ramdisk as output dir"

case $1 in
--help)    exec echo "$usage";;
--version) exec echo "$version";;
esac

INPUT_FILE=`readlink -f $1`

if [ ! -f "$INPUT_FILE" ]; then 
  echo -e "\033[31mErr: $1 does not exist\033[0m"
  echo ""
  exec echo "$usage"
  exit 1 
fi

if  [ ! -n "$2" ] ;then
  OUTPUT_DIR=./ramdisk
else
  OUTPUT_DIR=$2
fi

if [ ! -d "$OUTPUT_DIR" ]; then
  mkdir $OUTPUT_DIR
fi
echo "output to dir $OUTPUT_DIR"

cd $OUTPUT_DIR
exec gzip -d -c "$INPUT_FILE" | cpio -idu
