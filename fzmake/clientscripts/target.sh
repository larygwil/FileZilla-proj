#! /bin/sh

PACKAGES_FILE="$SCRIPTS/packages"
. "$SCRIPTS/readpackages"

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

  eval "$PREFIX/packages/$PACKAGE/configure" "'--prefix=$WORKDIR/prefix/$PACKAGE'" "'--host=$TARGET'" '--enable-static' '--disable-shared' $FLAGS || return 1
  if [ -z "$MAKE" ]; then
    make || return 1
    make install || return 1
  else
    "$MAKE" || return 1
    "$MAKE" install || return 1
  fi

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

