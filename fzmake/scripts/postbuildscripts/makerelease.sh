#! /bin/sh

makerelease()
{
  local RELEASEDIR="/home/nightlybuild/releases"

  rm -rf "$RELEASEDIR" > /dev/null 2>&1
  mkdir -p "$RELEASEDIR"
  mkdir -p "$RELEASEDIR/debug"

  local CONFIGUREIN="$WORKDIR/source/FileZilla3/configure.ac"

  local version=

  echo "Creating release files"

  exec 4<"$CONFIGUREIN" || return 1
  read <&4 -r version || return 1

  version="${version#*,[}"
  version="${version%%],*}"

  echo "Version: $version"

  cd "$OUTPUTDIR"

  cp "FileZilla3-src.tar.bz2" "$RELEASEDIR/FileZilla_${version}_src.tar.bz2"

  for TARGET in *; do
    if ! [ -f "$OUTPUTDIR/$TARGET/build.log" ]; then
      continue;
    fi
    if ! [ -f "$OUTPUTDIR/$TARGET/successful" ]; then
      continue;
    fi

    cd "$OUTPUTDIR/$TARGET"
    for i in FileZilla*; do
      local lext=${i#*.}
      local sext=${i##*.}

      if [ "$sext" = "sha512" ]; then
        continue
      fi

      if echo "$i" | grep debug > /dev/null; then
        local name="FileZilla_${version}_${TARGET}_debug.tar.bz2"

        cp "$i" "${RELEASEDIR}/debug/${name}"
        continue;
      fi

      local platform=
      local suffix=
      local hash=1
      case "$TARGET" in
        x86_64*mingw*)
          platform=win64
	  if [ "$lext" = "exe" ]; then
	    platform="${platform}-setup"
	  fi
          ;;
        i?86*mingw*)
          platform=win32
	  if [ "$lext" = "exe" ]; then
	    platform="${platform}-setup"
	  fi
          ;;
        *64-apple*|*86-apple*)
          platform="macosx-x86"
          ;;
        *)
          platform="$TARGET"
          ;;
      esac

      if echo "$i" | grep bundled > /dev/null; then
        suffix="_bundled"
        hash=0
      fi

      local name="FileZilla_${version}_${platform}${suffix}.$lext"

      cp "$i" "${RELEASEDIR}/${name}"

      if [ "$hash" = "1" ]; then
        pushd "${RELEASEDIR}" > /dev/null
        sha512sum --binary "${name}" >> "FileZilla_${version}.sha512"
        popd
      fi
    done
  done

}

makerelease
