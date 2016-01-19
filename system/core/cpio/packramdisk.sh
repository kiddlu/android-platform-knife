#!/bin/sh
# Pack ramdisk.img

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

version="packramdisk 1.0
Written by Kidd Lu."

usage="Usage: packramdisk dir [output file]... ...

if no output file, use default ./ramdisk.img as output file"

case $1 in
--help)    exec echo "$usage";;
--version) exec echo "$version";;
esac

INPUT=$1

if [ ! -d "$INPUT" ]; then 
  echo -e "\033[31mErr: $INPUT does not exist\033[0m"
  echo ""
  exec echo "$usage"
  exit 1 
fi

if  [ ! -n "$2" ] ;then
  OUTPUT=`pwd`/ramdisk.img
else
  OUTPUT=`readlink -f $2`
fi

echo "pack ramdisk to $OUTPUT"

exec mkbootfs "$INPUT" | minigzip > $OUTPUT
#cd `readlink -f $INPUT`;find . | cpio -o -H newc | gzip > $OUTPUT
