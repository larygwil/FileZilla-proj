#! /bin/sh

set -e

TARGET=$1
PACKAGE=$2

mkdir -p "$OUTPUTDIR/$TARGET"

if echo "$TARGET" | grep "mingw"; then
  cd "$WORKDIR/$PACKAGE/src/interface"
  strip -s filezilla.exe
  cd "$WORKDIR/$PACKAGE/src/putty"
  strip -s fzsftp.exe
  echo "Making installer"
  cd "$WORKDIR/$PACKAGE/data"

  # We don't need debug information for this file. Since it runs in the context of Explorer, it's pretty undebuggable anyhow.
  strip ../src/fzshellext/.libs/libfzshellext-0.dll
  
  makensis install.nsi
  chmod 775 FileZilla_3_setup.exe
  mv FileZilla_3_setup.exe "$OUTPUTDIR/$TARGET"

  sh makezip.sh "$WORKDIR/prefix/$PACKAGE" || return 1
  mv FileZilla.zip "$OUTPUTDIR/$TARGET/FileZilla.zip"
  
elif [ \( "$TARGET" = "i686-apple-darwin8" -o "$TARGET" = "powerpc-apple-darwin8" \) -a "$PACKAGE" = "FileZilla3" ]; then
  cd "$WORKDIR/$PACKAGE"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.app.tar.bz2" FileZilla.app
else
  cd "$WORKDIR/prefix"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" $PACKAGE
  bzip2 -t "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" || return 1
fi

