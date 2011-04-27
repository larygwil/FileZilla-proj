#! /bin/sh

makedist()
{
  logprint "Creating source distribution"
  logprint "Configuring FileZilla 3 package"

  mkdir -p "$WORKDIR/FileZilla3"
  
  cd "$WORKDIR/FileZilla3"
  ../source/FileZilla3/configure --enable-localesonly >> $LOG 2>&1 || return 1
  make dist >> $LOG 2>&1 || return 1

  cp `find *.tar.bz2` "$OUTPUTDIR/FileZilla3-src.tar.bz2" >> $LOG 2>&1 || return 1

  return 0
}

