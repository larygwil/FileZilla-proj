#! /bin/sh

NIGHTLYDIR=/var/www/nightlies
DAYS=3		# Minumum number of days to keep nightlies from

DATES=`ls $NIGHTLYDIR | grep "[0-9]\{4\}-[0-9]\{2\}-[0-9]\{2\}" | sort -r`

# List of targets which were compiled successfully
TARGETS=""

hasTarget()
{
  local TARGET=$1
  local i
  for i in $TARGETS; do
    if [ "$i" = "$TARGET" ]; then
      return 0
    fi
  done
  return 1
}

echo "Cleaning old nightlies"

for i in $DATES; do

  DELETE=1

  if [ "$DAYS" -gt "0" ]; then
    if [ "$DELETE" != "0" ]; then
      echo "Keeping $i, recent build"
    fi
    DELETE=0
    DAYS=`expr $DAYS - 1`
  fi

  cd "$NIGHTLYDIR/$i"
  for TARGET in *; do
    if ! [ -f "$TARGET/build.log" ]; then
      continue;
    fi
    if [ -e "$NIGHTLYDIR/$i/$TARGET/successful" ]; then
      if ! hasTarget $TARGET; then
        if [ "$DELETE" != "0" ]; then
	  echo "Keeping $i for $TARGET";
	fi
        DELETE=0
	TARGETS="$TARGETS $TARGET"
      fi
    fi
  done

  if [ "$DELETE" = "1" ]; then
    echo "Deleting nightly $i"
    rm -rf "$NIGHTLYDIR/$i"
  fi

done

