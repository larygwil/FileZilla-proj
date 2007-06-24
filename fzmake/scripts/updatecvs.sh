#! /bin/sh

function updatecvs()
{
  logprint "Updating CVS and SVN repositories"
  logprint "---------------------------------"
  
  resetPackages
  while getPackage; do
    PACKAGE=${PACKAGE#-}
    logprint "Updating package $PACKAGE"
    cd $CVSDIR/$PACKAGE
    if [ -d "CVS" ]; then
      cvs -q -z3 update -dP >> $LOG 2>&1 || return 1
    elif [ -d ".svn" ]; then
      svn update >> $LOG 2>&1 || return 1
    else
      logprint "Unknown repository type for package $PACKAGE"
      return 1
    fi
  done
}
