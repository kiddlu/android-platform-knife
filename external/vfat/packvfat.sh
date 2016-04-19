#!/bin/sh
# packvfat vfat.img

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

version="packvfat 1.0
Written by Kidd Lu."

usage="Usage: packvfat dir [output file]... ...

if no output file, use default ./vfat.img as output file"

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
  OUTPUT=`pwd`/vfat.img
else
  OUTPUT=`readlink -f $2`
fi
MOUNTPONIT=/tmp/vfat.dir.$RANDOM

SIZE=`du -sb $INPUT | awk '{print $1}'`
SIZE=`echo "$SIZE * 1.1 / 1" | bc`

#dd if=/dev/zero of=$OUTPUT count=1 bs=$SIZE > /dev/null
truncate -s $SIZE $OUTPUT
LOOPDEV=`losetup -f $OUTPUT --show`
mkdosfs -F 32  $LOOPDEV > /dev/null
mkdir $MOUNTPONIT
mount -t vfat $LOOPDEV $MOUNTPONIT

cp -drf $INPUT $MOUNTPONIT

umount $MOUNTPONIT
rm -rf $MOUNTPONIT
losetup -d $LOOPDEV

echo "OK!"