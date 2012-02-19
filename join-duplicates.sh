#!/bin/bash
#
# Example script for duff(1).
#
# Copyright (c) 2005 Ross Newell
#
# Modified Feb 12, 2012 by Camilla Berglund <elmindreda@elmindreda.org>
#
# Uses duff to find duplicate physical files and changes them into hard links
# to a single physical file, thus saving disk space.  Use with care.
#

if [ $# == 0 ]; then
  echo "Usage: `basename $0` directory [...]"
  exit 1
fi

duff -0Dprz -f '%n' -- "$@" |
(
  count=0
  while IFS='' read -d '' -r line 
  do
    if [ "$count" == 0 ]; then
      count="$line"
      first=''
    else
      if [ "$first" == '' ]; then
        first="$line"
      else
        file="$line"
	temp="`mktemp -p \`dirname $file\``"

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
      count="`expr $count - 1`"
    fi
  done
)

