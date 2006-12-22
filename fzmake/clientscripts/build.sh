#! /bin/sh

export PREFIX="$1"
export TARGET="$2"
export PACKAGES="$3"

export SCRIPTS="$PREFIX/clientscripts"
export WORKDIR="$PREFIX/work"
export OUTPUTDIR="$PREFIX/output"

echo "Clientscript forked"
echo "Making sure environment is sane"

# Adding $TARGET's prefix
if [ ! -z "$HOME" ]; then
  if [ -d "$HOME/prefix-$TARGET" ]; then
    if [ -z "$PATH" ]; then
      export PATH="$HOME/prefix-$TARGET/bin"
    else
      export PATH="$HOME/prefix-$TARGET/bin:$PATH"
    fi
    if [ -z "$CPPFLAGS" ]; then
      export CPPFLAGS="-I$HOME/prefix-$TARGET/include"
    else
      export CPPFLAGS="-I$HOME/prefix-$TARGET/include $CPPFLAGS"
    fi
    if [ -z "$LDFLAGS" ]; then
      export LDFLAGS="-L$HOME/prefix-$TARGET/lib"
    else
      export LDFLAGS="-L$HOME/prefix-$TARGET/lib $LDFLAGS"
    fi
    if [ -z "$LD_LIBRARY_PATH" ]; then
      export LD_LIBRARY_PATH="$HOME/prefix-$TARGET/lib"
    else
      export LD_LIBRARY_PATH="$HOME/prefix-$TARGET/lib:$LD_LIBRARY_PATH"
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


