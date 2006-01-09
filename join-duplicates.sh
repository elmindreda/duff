#!/bin/sh
#
# Example script for duff(1).
#
# Copyright (c) 2005 "Snow"
#
# Modified Jan 7, 2006 by Camilla Berglund <elmindreda@users.sourceforge.net>
#
# Uses duff to find duplicate physical files and changes them into hard links
# to a single physical file, thus saving disk space.  Use with care.
#

if [ "$1" == '' ]; then
  echo "usage: `basename $0` directory"
  exit 1
fi

duff -r '-f#' -z -p -P "$1" |
(
  while read file 
  do
    if [ "$file" == '#' ]; then
      first=''
    else
      if [ "$first" == '' ]; then
        first="$file"
      else
	temp=`mktemp -p \`dirname $file\``

	mv "$file" "$temp" && \
	ln "$first" "$file" && \
	touch --reference="$temp" "$file" && \
	rm "$temp"

	if [ $? != 0 ]; then
	  echo "`basename $0`: $file: failed to join with $first"
	  echo "`basename $0`: $file: may exist as $temp"
	  exit 1
	fi
      fi
    fi
  done
)

