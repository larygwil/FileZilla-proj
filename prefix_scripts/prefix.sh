#! /bin/sh

export PREFIX="$1"

if [ -z "$OLDPATH" ]; then
  export OLDPATH="$PATH"
fi

unset LD_LIBRARY_PATH
unset CPPFLAGS
unset LDFLAGS
export PATH="$OLDPATH"
unset PKG_CONFIG_PATH
unset CONFIGURE_ARGS
unset WINEARCH
unset WINEPREFIX

if [ -z "$PREFIX" ]; then
  unset PREFIX
  echo "Prefix has been unset"
  return 0
else
  if ! [ -d "$PREFIX" ]; then
    if [ -d "prefix-$PREFIX" ]; then
      PREFIX="prefix-$PREFIX"
    elif [ -d "$HOME/prefix-$PREFIX" ]; then
      PREFIX="$HOME/prefix-$PREFIX"
    else
      echo "Invalid prefix"
      unset PREFIX
      return 1
    fi
  fi
  PREFIX=`realpath "$PREFIX"`

  if [ ! -d "$PREFIX" ]; then
    echo "Invalid prefix"
    unset PREFIX
    return 1
  fi

  export PATH="$PREFIX/bin:$OLDPATH"
  export LD_LIBRARY_PATH="$PREFIX/lib"
  export CPPFLAGS="-I$PREFIX/include"
  export LDFLAGS="-L$PREFIX/lib"
  export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"

  if [ -x "$PREFIX/profile" ]; then
    . "$PREFIX/profile"
  fi

  export CONFIGURE_ARGS="$CONFIGURE_ARGS --prefix=\"$PREFIX\""

  echo "Prefix set to $PREFIX"
fi
