#! /bin/bash

export PREFIX="$1"
export TARGET="$2"

export SCRIPTS="$PREFIX/clientscripts"
export WORKDIR="$PREFIX/work"
export OUTPUTDIR="$PREFIX/output"

echo "Clientscript forked"
echo "Making sure environment is sane"

safe_prepend()
{
  local VAR=$1
  local VALUE=$2
  local OLD

  eval OLD=\$$VAR:
  while [ ! -z "$OLD" ]; do

    FIRST=${OLD%%:*}
    if [ "$FIRST" = "$VALUE" ]; then
      return
    fi

    OLD=${OLD#*:}
  done 

  eval export $VAR=$VALUE\${$VAR:+:}\$$VAR
}

# Adding $TARGET's prefix
if [ ! -z "$HOME" ]; then
  if [ -d "$HOME/prefix-$TARGET" ]; then
    safe_prepend PATH "$HOME/prefix-$TARGET/bin"
    safe_prepend CPPFLAGS "-I$HOME/prefix-$TARGET/include"
    safe_prepend LDFLAGS "-L$HOME/prefix-$TARGET/lib"
    safe_prepend LD_LIBRARY_PATH "$HOME/prefix-$TARGET/lib"

    if [ -f "$HOME/prefix-$TARGET/CFLAGS" ]; then
      safe_prepend CFLAGS "`cat \"$HOME/prefix-$TARGET/CFLAGS\"`"
    fi
    if [ -f "$HOME/prefix-$TARGET/CXXFLAGS" ]; then
      safe_prepend CXXFLAGS "`cat \"$HOME/prefix-$TARGET/CXXFLAGS\"`"
    fi
  fi
fi

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

echo "Cleanup"
rm -rf "$WORKDIR"


