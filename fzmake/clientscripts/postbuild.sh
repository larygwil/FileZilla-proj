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

do_strip()
{
  if [ "$STRIP" = "false" ]; then
    return
  fi

  if [ -d "$3" ]; then
    if [ -f "$1/.libs/$2" ]; then
      cp "$1/.libs/$2" "$3/$2"
    else
      cp "$1/$2" "$3/$2"
    fi
  fi

  if [ -f "$1/$2" ]; then
    if ! "$STRIP" "$1/$2"; then
      echo "Stripping of \"$1/$2\" failed."
      exit 1
    fi
  fi
  if [ -f "$1/.libs/$2" ]; then
    if ! "$STRIP" "$1/.libs/$2"; then
      echo "Stripping of \"$1/.libs/$2\" failed."
      exit 1
    fi
  fi
}

do_sign()
{
  if ! [ -x "$HOME/prefix-$TARGET/sign.sh" ]; then
    return
  fi

  echo "Signing $1/$2"
  if [ -f "$1/.libs/$2" ]; then
    "$HOME/prefix-$TARGET/sign.sh" "$1/.libs/$2" || exit 1
  else
    "$HOME/prefix-$TARGET/sign.sh" "$1/$2" || exit 1
  fi
}

rm -rf "$WORKDIR/debug"
mkdir "$WORKDIR/debug"

if echo "$TARGET" | grep "mingw"; then
  do_strip "$WORKDIR/$PACKAGE/src/interface" "filezilla.exe" "$WORKDIR/debug"
  do_strip "$WORKDIR/$PACKAGE/src/putty" "fzputtygen.exe" "$WORKDIR/debug"
  do_strip "$WORKDIR/$PACKAGE/src/putty" "fzsftp.exe" "$WORKDIR/debug"
  do_strip "$WORKDIR/$PACKAGE/src/fzshellext/32" "libfzshellext-0.dll" "$WORKDIR/debug"
  do_strip "$WORKDIR/$PACKAGE/src/fzshellext/64" "libfzshellext-0.dll" "$WORKDIR/debug"

  do_sign "$WORKDIR/$PACKAGE/src/interface" "filezilla.exe"
  do_sign "$WORKDIR/$PACKAGE/src/putty" "fzputtygen.exe"
  do_sign "$WORKDIR/$PACKAGE/src/putty" "fzsftp.exe"
  do_sign "$WORKDIR/$PACKAGE/src/fzshellext/32" "libfzshellext-0.dll"
  do_sign "$WORKDIR/$PACKAGE/src/fzshellext/64" "libfzshellext-0.dll"

  echo "Making installer"
  cd "$WORKDIR/$PACKAGE/data"

  wine /home/nightlybuild/nsis-3.0b3/makensis.exe install.nsi
  wine /home/nightlybuild/nsis-3.0b3/makensis.exe /DENABLE_OFFERS install.nsi

  do_sign "$WORKDIR/$PACKAGE/data" "FileZilla_3_setup.exe"
  do_sign "$WORKDIR/$PACKAGE/data" "FileZilla_3_setup_bundled.exe"

  chmod 775 FileZilla_3_setup.exe
  mv FileZilla_3_setup.exe "$OUTPUTDIR/$TARGET"
  chmod 775 FileZilla_3_setup_bundled.exe
  mv FileZilla_3_setup_bundled.exe "$OUTPUTDIR/$TARGET"

  sh makezip.sh "$WORKDIR/prefix/$PACKAGE" || exit 1
  mv FileZilla.zip "$OUTPUTDIR/$TARGET/FileZilla.zip"

  cd "$OUTPUTDIR/$TARGET" || exit 1
  sha512sum --binary "FileZilla_3_setup.exe" > "FileZilla_3_setup.exe.sha512" || exit 1
  sha512sum --binary "FileZilla.zip" > "FileZilla.zip.sha512" || exit 1

elif echo "$TARGET" | grep apple-darwin 2>&1 > /dev/null; then

  cd "$WORKDIR/$PACKAGE"
  do_strip "FileZilla.app/Contents/MacOS" filezilla "$WORKDIR/debug"
  do_strip "FileZilla.app/Contents/MacOS" fzputtygen "$WORKDIR/debug"
  do_strip "FileZilla.app/Contents/MacOS" fzsftp "$WORKDIR/debug"

  if [ -x "$HOME/prefix-$TARGET/sign.sh" ]; then
    echo "Signing bundle"
    "$HOME/prefix-$TARGET/sign.sh" || exit 1
  fi

  echo "Creating ${PACKAGE}.app.tar.bz2 tarball"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.app.tar.bz2" FileZilla.app

  cd "$OUTPUTDIR/$TARGET" || exit 1
  sha512sum --binary "$PACKAGE.app.tar.bz2" > "$PACKAGE.app.tar.bz2.sha512" || exit 1

else

  cd "$WORKDIR/prefix"
  do_strip "$PACKAGE/bin" filezilla "$WORKDIR/debug"
  do_strip "$PACKAGE/bin" fzputtygen "$WORKDIR/debug"
  do_strip "$PACKAGE/bin" fzsftp "$WORKDIR/debug"
  tar -cjf "$OUTPUTDIR/$TARGET/$PACKAGE.tar.bz2" $PACKAGE || exit 1
  cd "$OUTPUTDIR/$TARGET" || exit 1
  sha512sum --binary "$PACKAGE.tar.bz2" > "$PACKAGE.tar.bz2.sha512" || exit 1

fi

if [ "$STRIP" != "false" ]; then
  echo "Creating debug package"
  cd "$WORKDIR" || exit 1
  tar cjf "$OUTPUTDIR/$TARGET/${PACKAGE}_debug.tar.bz2" "debug" || exit 1
fi
rm -rf "$WORKDIR/debug"

