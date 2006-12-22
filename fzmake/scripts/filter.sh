#! /bin/sh

function dofilter()
{
  PREFIX="$HOSTPREFIX"
  PREFIX=`echo "$PREFIX" | sed -e 's/\\//\\\\\\//g'`

  while read -s -u 0; do
    echo "$REPLY" | sed "s/$PREFIX/\$PREFIX/g"
  done
}

function filter()
{
  "$@" 2>&1 | dofilter
  return ${PIPESTATUS[0]}
}

#export HOSTPREFIX=a
#filter echo '****'
