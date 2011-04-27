#! /bin/sh

#while read -u 0; do
#  echo "REPLY: $REPLY"
#done

logprint()
{
  echo -e $*
  echo -e $* >> $LOG
}

targetlogprint()
{
  echo -e "$TARGET: $*"
  echo -e $* >> $TARGETLOG
}

