#! /bin/sh

makerelease()
{
  local RELEASEDIR="/home/nightlybuild/releases"

  rm -rf "$RELEASEDIR" > /dev/null 2>&1
  mkdir -p "$RELEASEDIR"

  local CONFIGUREIN="$WORKDIR/source/FileZilla3/configure.in"

  local version=

  echo "Creating release files"

  exec 4<"$CONFIGUREIN" || return 1
  read -u 4 version || return 1

  version="${version#*, }"
  version="${version%,*}"

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
      local ext=${i#*.}

      if [ "$ext" = "sha512" ]; then
        continue
      fi

      locale platform=
      case "$TARGET" in
        i586-mingw32msvc)
          platform=win32
	  if [ "$ext" = "exe" ]; then
	    platform="${platform}-setup"
	  fi
          ;;
        *)
          platform="$TARGET"
          ;;
      esac

      local name="FileZilla_${version}_${platform}.$ext"
      echo $name

      cp "$i" "${RELEASEDIR}/${name}"
    done
  done

  cd ${RELEASEDIR}

  sha512sum --binary * > "FileZilla_${version}.sha512"
}

makerelease
