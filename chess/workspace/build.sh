#! /bin/bash

set -e

MAKEOPTS=-j4

if [ -z "$REV" ]; then
  REV=`svn info . | grep '^Revision: ' | grep -o '[0-9]\+'`
fi

if [ -z "$REV" ]; then
  echo "Could not get SVN revision"
  exit 1
fi

rm -rf octochess-r$REV
mkdir -p octochess-r$REV

make clean
make ${MAKEOPTS} bookgen
./bookgen fold
./bookgen "export octochess-r$REV/octochess.book"

make clean
make ${MAKEOPTS} -f Makefile-opt chess-use REVISION="-DREVISION=\\\"$REV\\\""
strip chess-use
cp chess-use octochess-r$REV/octochess-linux-sse4-r$REV

rm -f chess-use
make clean
make ${MAKEOPTS} -f Makefile-opt ARCH=core2 chess-use REVISION="-DREVISION=\\\"$REV\\\"" EXTRA_CPPFLAGS="-DUSE_GENERIC_POPCOUNT=1"
strip chess-use
cp chess-use octochess-r$REV/octochess-linux-generic-r$REV

cp AUTHORS octochess-r$REV/authors.txt
cp COPYING octochess-r$REV/copying.txt
cp NEWS octochess-r$REV/news.txt
cp README octochess-r$REV/readme.txt
cp logo.bmp octochess-r$REV/
cp logo.svg octochess-r$REV/

unix2dos --force octochess-r$REV/*.txt
