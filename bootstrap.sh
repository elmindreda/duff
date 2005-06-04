#!/bin/sh

# This script performs the necessary steps to get from
# a clean CVS checkout to where you can run configure.

aclocal && \
autoheader && \
automake -a && \
automake && \
autoconf && \
echo "Bootstrap successful"

