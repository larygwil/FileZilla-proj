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

  # Convert slashes into backslashes or makensis will complain
  cat install.nsi | sed '/define top_srcdir/s/\//\\/g' > install.nsi2
  cat install.nsi2 | sed '/define srcdir/s/\//\\/g' > install.nsi

  # Prepare files for Unicode NSIS
  # See http://www.scratchpaper.com/ for details
  cat install.nsi | sed 's/${top_srcdir}\\COPYING/COPYING/' > install.nsi2
  cat install.nsi2 | iconv -f utf8 -t utf16 > install.nsi
  rm install.nsi2
  cat "$PREFIX/packages/FileZilla3/COPYING" | iconv -f utf8 -t utf16 > COPYING

  # makensis install.nsi
  wine /home/nightlybuild/NSIS_unicode/makensis.exe install.nsi

  chmod 775 FileZilla_3_setup.exe
  mv FileZilla_3_setup.exe "$OUTPUTDIR/$TARGET"

  sh makezip.sh "$WORKDIR/prefix/$PACKAGE" || return 1
  mv FileZilla.zip "$OUTPUTDIR/$TARGET/FileZilla.zip"
  
elif [ \( "$TARGET" = "i686-apple-darwin9" -o "$TARGET" = "powerpc-apple-darwin9" \) -a "$PACKAGE" = "FileZilla3" ]; then
  cd "$WORKDIR/$PACKAGE"
  strip FileZilla.app/Contents/MacOS/filezilla
  strip FileZilla.app/Contents/MacOS/fzsftp
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.app.tar.bz2" FileZilla.app
else
  cd "$WORKDIR/prefix"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" $PACKAGE
  bzip2 -t "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" || return 1
fi

