#! /bin/bash

set -e

REV=`svn info . | grep '^Revision: ' | grep -o '[0-9]\+'`

if [ "$REV" = "" ]; then
  echo "Could not get SVN revision"
  exit 1
fi

rm -rf octochess-r$REV
mkdir -p octochess-r$REV

make clean
make -f Makefile-opt chess-use
cp chess-use octochess-r$REV/octochess-linux-sse4-r$REV

rm -f chess-use
make clean
make -f Makefile-opt ARCH=core2 chess-use REVISION="-DREVISION=\\\"$REV\\\""
cp chess-use octochess-r$REV/octochess-linux-generic-r$REV

