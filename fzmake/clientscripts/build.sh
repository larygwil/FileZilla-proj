#! /bin/bash

export PREFIX="$1"
export TARGET="$2"
CHROOTED="$3"

export SCRIPTS="$PREFIX/clientscripts"
export WORKDIR="$PREFIX/work"
export OUTPUTDIR="$PREFIX/output"

echo "Clientscript forked"
echo "Making sure environment is sane"

if which caffeinate > /dev/null 2>&1; then
  # Prevent OS X from falling asleep
  caffeinate -i -w $$ &
fi

. "$SCRIPTS/parameters"

safe_prepend()
{
  local VAR=$1
  local VALUE=$2
  local SEPARATOR=$3
  local OLD

  # Make sure it's terminated by a separator
  eval OLD=\"\$$VAR$SEPARATOR\"
  while [ ! -z "$OLD" ]; do

    # Get first segment
    FIRST="${OLD%%$SEPARATOR*}"
    if [ "$FIRST" = "$VALUE" ]; then
      return
    fi

    # Strip first
    OLD="${OLD#*$SEPARATOR}"
  done

  eval export $VAR=\"$VALUE\${$VAR:+$SEPARATOR}\$$VAR\"
}

if ! [ -d "$PREFIX" ]; then
  echo "\$PREFIX does not exist, check config/hosts"
  exit 1
fi

# Adding $TARGET's prefix
if [ ! -z "$HOME" ]; then
  if [ -d "$HOME/prefix-$TARGET" ]; then

    echo "Found target specific prefix: \$HOME/prefix-$TARGET"

    if [ -f "$HOME/prefix-$TARGET/SCHROOT" ]; then
      if [ -z "$CHROOTED" ]; then
        echo "Changing root directory"
        schroot -c "`cat \"$HOME/prefix-$TARGET/SCHROOT\"`" $0 "$PREFIX" "$TARGET" 1
        echo "Returned from chroot"
        exit $?
      fi
    fi

    if [ -x "$HOME/prefix-$TARGET/profile" ]; then
      . "$HOME/prefix-$TARGET/profile"
    fi

    safe_prepend PATH "$HOME/prefix-$TARGET/bin" ':'
    safe_prepend CPPFLAGS "-I$HOME/prefix-$TARGET/include" ' '
    safe_prepend LDFLAGS "-L$HOME/prefix-$TARGET/lib" ' '
    safe_prepend LD_LIBRARY_PATH "$HOME/prefix-$TARGET/lib" ':'
    safe_prepend PKG_CONFIG_PATH "$HOME/prefix-$TARGET/lib/pkgconfig" ':'
  fi
fi

echo "CPPFLAGS: $CPPFLAGS"
echo "CFLAGS: $CFLAGS"
echo "CXXFLAGS: $CXXFLAGS"
echo "LDFLAGS: $LDFLAGS"
echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"
echo "PKG_CONFIG_PATH: $PKG_CONFIG_PATH"

rm -rf "$OUTPUTDIR"
mkdir "$OUTPUTDIR"

if [ -e "$HOME/.bash_profile" ]; then
  . "$HOME/.bash_profile"
fi

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"

echo "Making target $TARGET"
$SCRIPTS/target.sh $TARGET || exit 1

echo "Making tarball of output files"
cd "$OUTPUTDIR"
tar -cf "$PREFIX/output.tar" * || exit 1

SIZE=`ls -nl "$PREFIX/output.tar" | awk '{print $5}'`
echo "Size of output: $SIZE bytes"

echo "Cleanup"
rm -rf "$WORKDIR"


