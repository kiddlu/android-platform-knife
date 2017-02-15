#!/bin/sh
# sefcontext decompiler

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

version="sefcontext_decompile 1.0
Written by Kidd Lu."

usage="Usage: sefcontext_decompile file_contexts.bin [output]... ...

if no output, use default ./file_contexts as output"

case $1 in
    --help)    exec echo "$usage";;
    --version) exec echo "$version";;
esac

INPUT=$1

if [ ! -f "$INPUT" ]; then 
    echo -e "\033[31mErr: $INPUT does not exist\033[0m"
    echo ""
    exec echo "$usage"
    exit 1
fi

if  [ ! -n "$2" ] ;then
    OUTPUT=`pwd`/file_contexts
else
    OUTPUT=`readlink -f $2`
fi

echo "decompile $INPUT to $OUTPUT"

context_r=`mktemp --suffix=.tmp`
context_l=`mktemp --suffix=.tmp`
trap "rm -f $context_l $context_r" EXIT

strings $INPUT | sed -n '/u:/ {n;p}' > $context_l
strings $INPUT | sed -n '/u:/ {p}'   > $context_r
paste $context_l $context_r | grep -v ERCP > $OUTPUT
sed -i '1 i\/ u:object_r:rootfs:s0' $OUTPUT
