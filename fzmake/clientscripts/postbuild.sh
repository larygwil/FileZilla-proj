#! /bin/sh

set -e

TARGET=$1
PACKAGE=$2

mkdir -p "$OUTPUTDIR/$TARGET"

if [ "$STRIP" = "false" ]; then
  echo "Binaries will not be stripped"
elif [ "$STRIP" = "" ]; then
  export STRIP=`which strip`
  echo "Binaries will be stripped using default \"$STRIP\""
  if [ ! -x "$STRIP" ]; then
    echo "Stripping program does not exist or is not marked executable."
    exit 1
  fi
else
  export STRIP=`which "$STRIP"`
  echo "Binaries will be stripped using \"$STRIP\""
  if [ ! -x "$STRIP" ]; then
    echo "Stripping program does not exist or is not marked executable."
    exit 1
  fi
fi

libtool_strip()
{
  if [ "$STRIP" = "false" ]; then
    return
  fi

  if ! "$STRIP" "$1/$2"; then
    echo "Stripping of \"$1/$2\" failed."
    exit 1
  fi
  if [ -f "$1/.libs/$2" ]; then
    if ! "$STRIP" "$1/.libs/$2"; then
      echo "Stripping of \"$1/.libs/$2\" failed."
      exit 1
    fi
  fi
}

if echo "$TARGET" | grep "mingw"; then
  libtool_strip "$WORKDIR/$PACKAGE/src/interface" "filezilla.exe"
  libtool_strip "$WORKDIR/$PACKAGE/src/putty" "fzputtygen.exe"
  libtool_strip "$WORKDIR/$PACKAGE/src/putty" "fzsftp.exe"
  # libtool_strip "$WORKDIR/$PACKAGE/src/fzshellext" "libfzshellext-0.dll"

  # We don't need debug information for this file. Since it runs in the context of Explorer, it's pretty undebuggable anyhow.
  [ "$STRIP" != "false" ] && "$STRIP" "$WORKDIR/$PACKAGE/src/fzshellext/.libs/libfzshellext-0.dll"

  echo "Making installer"
  cd "$WORKDIR/$PACKAGE/data"

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

  sh makezip.sh "$WORKDIR/prefix/$PACKAGE" || exit 1
  mv FileZilla.zip "$OUTPUTDIR/$TARGET/FileZilla.zip"

  cd "$OUTPUTDIR/$TARGET" || exit 1
  sha512sum --binary "FileZilla_3_setup.exe" > "FileZilla_3_setup.exe.sha512" || exit 1
  sha512sum --binary "FileZilla.zip" > "FileZilla.zip.sha512" || exit 1

elif [ \( "$TARGET" = "i686-apple-darwin9" -o "$TARGET" = "powerpc-apple-darwin9" \) -a "$PACKAGE" = "FileZilla3" ]; then

  cd "$WORKDIR/$PACKAGE"
  [ "$STRIP" != "false" ] && "$STRIP" -S -x FileZilla.app/Contents/MacOS/filezilla
  [ "$STRIP" != "false" ] && "$STRIP" -S -x FileZilla.app/Contents/MacOS/fzputtygen
  [ "$STRIP" != "false" ] && "$STRIP" -S -x FileZilla.app/Contents/MacOS/fzsftp
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.app.tar.bz2" FileZilla.app

  cd "$OUTPUTDIR/$TARGET" || exit 1
  sha512sum --binary "$PACKAGE.app.tar.bz2" > "$PACKAGE.app.tar.bz2.sha512" || exit 1

else

  cd "$WORKDIR/prefix"
  [ "$STRIP" != "false" ] && "$STRIP" -g "$PACKAGE/bin/filezilla"
  [ "$STRIP" != "false" ] && "$STRIP" -g "$PACKAGE/bin/fzsftp"
  [ "$STRIP" != "false" ] && "$STRIP" -g "$PACKAGE/bin/fzputtygen"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" $PACKAGE || exit 1
  cd "$OUTPUTDIR/$TARGET" || exit 1
  sha512sum --binary "$PACKAGE.tar.bz2" > "$PACKAGE.tar.bz2.sha512" || exit 1

fi

