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

  HOSTARG=`"$SCRIPTS/configure_target.sh" "$TARGET" "$HOST"`

  top_srcdir=$(relpath "$(pwd)" "$PREFIX/packages/$PACKAGE/")

  if ! eval "${top_srcdir}/configure" "'--prefix=$WORKDIR/prefix/$PACKAGE'" $HOSTARG $FLAGS; then
    cat config.log
    return 1
  fi
  if [ -z "$MAKE" ]; then
    MAKE=make
  fi

  echo "Build command: nice \"$MAKE\" -j`cpu_count`"
  nice "$MAKE" -j`cpu_count` || return 1
  if grep '^check:' Makefile >/dev/null 2>&1; then
    if ! nice "$MAKE" -j`cpu_count` check; then
      if [ -f tests/test-suite.log ]; then
        cat tests/test-suite.log
      fi
      exit 1
    fi
  fi
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
  LD_LIBRARY_PATH="$WORKDIR/prefix/$PACKAGE/lib:$LD_LIBRARY_PATH"
  PKG_CONFIG_PATH="$WORKDIR/prefix/$PACKAGE/lib/pkgconfig:$PKG_CONFIG_PATH"
done

