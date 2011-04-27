#! /bin/sh

dofilter()
{
  PREFIX="$HOSTPREFIX"
  PREFIX=`echo "$PREFIX" | sed -e 's/\\//\\\\\\//g'`

  while read -s -r REPLY; do
    echo "$REPLY" | sed "s/$PREFIX/\$PREFIX/g"
  done
}

filter()
{
  "$@" 2>&1 | dofilter
  return ${PIPESTATUS[0]}
}

#export HOSTPREFIX=a
#filter echo '****'
