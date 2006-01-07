#!/bin/sh
#
# Example script for duff(1).
#
# Copyright (c) 2005 "Snow"
#
# Modified Jan 6, 2006 by Camilla Berglund <elmindreda@users.sourceforge.net>
#
# Uses duff to find duplicate physical files and changes them into hard links
# to a single physical file, thus saving disk space.  Use with care.
#

if [ -z "$1" ]; then
  echo "usage: `basename $0` directory"
  exit 1
fi

echo "## `date`" >> dupe.log

duff -r '-f#' -z -P -p "$1" |
(
  while read line 
  do
    if [ "$line" == '#' ]; then
      first=''
    else
      if [ "$first" == '' ]; then
        first="$line"
      else
	temp=`mktemp -p \`dirname $line\``

	#echo "$line" > dupe.name
	#echo "$first $line" >> dupe.log

	mv "$line" "$temp" && \
	ln "$first" "$line" && \
	rm "$temp"

	if [ $? != "0" ]; then
	  echo "`basename $0`: failed to link $line to $first (may exist as $temp)"
	  exit 1
	fi

	touch --reference="$temp" "$line"
      
        # Why do this?
	if [ "$temp" -nt "$first" ]; then
	  chmod --reference="$temp" "$first";
	  chown --reference="$temp" "$first";
	fi

	#rm dupe.name
      fi
    fi
  done
)

