#! /bin/sh

updatecvs()
{
  logprint "Updating CVS and SVN repositories"
  logprint "---------------------------------"
  
  resetPackages || return 1
  while getPackage; do
    PACKAGE=${PACKAGE#-}
    logprint "Updating package $PACKAGE"

    if ! [ -d "$CVSDIR/$PACKAGE" ]; then
      logprint "Error: $CVSDIR/$PACKAGE does not exist"
      return 1
    fi

    cd $CVSDIR/$PACKAGE
    if [ -d "CVS" ]; then
      cvs -q -z3 update -dP >> $LOG 2>&1 || return 1
    elif [ -d ".svn" ]; then
      svn update >> $LOG 2>&1 && continue
      svn update >> $LOG 2>&1 || return 1
    else
      logprint "Unknown repository type for package $PACKAGE"
      return 1
    fi
  done
}
