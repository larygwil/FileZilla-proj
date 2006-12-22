#! /bin/sh

function updatecvs()
{
  logprint "Updating CVS"
  logprint "------------"
  
  for i in $PACKAGES; do
    i=${i#-}
    logprint "Updating package $i"
    cd $CVSDIR/$i
    cvs -q -z3 update -dP >> $LOG 2>&1 || return 1
  done
}
