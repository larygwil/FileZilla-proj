#! /bin/bash

set -e

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
make -f Makefile-opt chess-use
cp chess-use octochess-r$REV/octochess-linux-sse4-r$REV

rm -f chess-use
make clean
make -f Makefile-opt ARCH=core2 chess-use REVISION="-DREVISION=\\\"$REV\\\""
cp chess-use octochess-r$REV/octochess-linux-generic-r$REV

strip octochess-r$REV/*

cp opening_book.db octochess-r$REV/
sqlite3 octochess-r$REV/opening_book.db 'delete from position where data IS NULL;'
sqlite3 octochess-r$REV/opening_book.db 'vacuum full'

cp AUTHORS octochess-r$REV/authors.txt
cp COPYING octochess-r$REV/copying.txt
cp NEWS octochess-r$REV/news.txt
cp README octochess-r$REV/readme.txt

unix2dos --force octochess-r$REV/*.txt
