#! /bin/sh

export TARGET=$1

makepackage()
{
  PACKAGE=$1
  NOINST=
  if [ "$PACKAGE" != "${PACKAGE#-}" ]; then
    NOINST="yes"
  fi
  PACKAGE=${PACKAGE#-}

  echo "Building $PACKAGE for $TARGET"

  mkdir -p "$WORKDIR/$PACKAGE"
  cd "$WORKDIR/$PACKAGE"

  special=
  if [ "$TARGET" = "i586-mingw32msvc" ]; then
    special="--disable-precomp-headers"
  elif [ "$TARGET" = "i386-pc-freebsd5.4" ]; then
    special="--disable-precomp-headers"
  fi

  $PREFIX/packages/$PACKAGE/configure --prefix="$WORKDIR/prefix/$PACKAGE" --host=$TARGET --enable-static --disable-shared --enable-unicode --without-libtiff --without-libjpeg --enable-buildtype=nightly $special || return 1
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

for i in $PACKAGES; do

  makepackage $i || exit 1

  package=${i#-}

  PATH="$WORKDIR/prefix/$package/bin:$PATH"
  LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$WORKDIR/prefix/$package/lib"
done

