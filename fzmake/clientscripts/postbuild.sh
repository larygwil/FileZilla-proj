#! /bin/sh

set -e

TARGET=$1
PACKAGE=$2

mkdir -p "$OUTPUTDIR/$TARGET"

if echo "$TARGET" | grep "mingw"; then
  cd "$WORKDIR/$PACKAGE/src/interface"
  strip -s filezilla.exe
  echo "Making installer"
  cd "$WORKDIR/$PACKAGE/data"
  makensis install.nsi
  chmod 775 FileZilla_3_setup.exe
  mv FileZilla_3_setup.exe "$OUTPUTDIR/$TARGET"
elif [ \( "$TARGET" = "i686-apple-darwin8" -o "$TARGET" = "powerpc-apple-darwin8" \) -a "$PACKAGE" = "FileZilla3" ]; then
  cd "$WORKDIR/$PACKAGE"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.app.tar.bz2" FileZilla.app
else
  cd "$WORKDIR/prefix"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" $PACKAGE
  bzip2 -t "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" || return 1
fi

