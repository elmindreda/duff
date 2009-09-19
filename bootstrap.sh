#!/bin/sh

# This script performs the necessary steps to get from
# a clean CVS checkout to where you can run configure.

(
  if [ -n "`which autoreconf`" ]; then
    autoreconf -i
  elif [ -n "`which autoreconf259`" ]; then
    autoreconf259 -i
  else
    echo "autoreconf not found"
    exit 1
  fi
) && \
echo "Bootstrap successful; now run configure"

