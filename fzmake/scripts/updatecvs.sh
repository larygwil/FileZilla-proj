#! /bin/sh

function updatecvs()
{
  logprint "Updating CVS"
  logprint "------------"
  
  resetPackages
  while getPackage; do
    PACKAGE=${PACKAGE#-}
    logprint "Updating package $PACKAGE"
    cd $CVSDIR/$PACKAGE
    cvs -q -z3 update -dP >> $LOG 2>&1 || return 1
  done
}
