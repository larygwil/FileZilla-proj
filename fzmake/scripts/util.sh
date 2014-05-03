#! /bin/sh

cpu_count()
{
  NPROC=`which nproc 2>/dev/null` 
  if [ ! -z "$NPROC" ] && [ -x "$NPROC" ]; then
    echo `$NPROC`
    return
  fi

  NPROC=`sysctl hw.ncpu 2>/dev/null | sed 's/.* //' 2>/dev/null`
  if [ ! -z "$NPROC" ]; then
    echo $NPROC
    return
  fi

  echo 1
}
