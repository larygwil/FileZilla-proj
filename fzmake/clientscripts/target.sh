#! /bin/sh

PACKAGES_FILE="$SCRIPTS/packages"
. "$SCRIPTS/readpackages"
. "$SCRIPTS/util.sh"

export TARGET=$1

makepackage()
{
  PACKAGE=$1
  FLAGS=$2

  NOINST=
  if [ "$PACKAGE" != "${PACKAGE#-}" ]; then
    NOINST="yes"
  fi
  PACKAGE=${PACKAGE#-}

  echo "Building $PACKAGE for $TARGET"

  mkdir -p "$WORKDIR/$PACKAGE"
  cd "$WORKDIR/$PACKAGE"

  HOST=`"$SCRIPTS/configure_target.sh" "$TARGET"`
  if ! eval "$PREFIX/packages/$PACKAGE/configure" "'--prefix=$WORKDIR/prefix/$PACKAGE'" $HOST $FLAGS; then
    cat config.log
    return 1
  fi
  if [ -z "$MAKE" ]; then
    MAKE=make
  fi

  nice "$MAKE" -j`cpu_count` || return 1
  nice "$MAKE" install || return 1

  if [ -z "$NOINST" ]; then
    echo "Running postbuild script"
    $SCRIPTS/postbuild.sh "$TARGET" "$PACKAGE" || return 1
  fi
}

while getPackage; do
  makepackage $PACKAGE "$PACKAGE_FLAGS" || exit 1

  PACKAGE=${PACKAGE#-}

  PATH="$WORKDIR/prefix/$PACKAGE/bin:$PATH"
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$WORKDIR/prefix/$PACKAGE/lib"
done

