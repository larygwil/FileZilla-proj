#! /bin/sh

#while read -u 0; do
#  echo "REPLY: $REPLY"
#done

function logprint()
{
  echo -e $*
  echo -e $* >> $LOG
}

function targetlogprint()
{
  echo -e "$TARGET: $*"
  echo -e $* >> $TARGETLOG
}

