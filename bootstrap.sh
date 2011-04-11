#!/bin/sh

# This script performs the necessary steps to get from
# a clean Git checkout to where you can run configure.

(
  if [ -n "`which gettextize`" ]; then
    gettextize
  else
    echo "gettextize not found"
    exit 1
  fi
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

