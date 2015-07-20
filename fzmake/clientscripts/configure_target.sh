#! /bin/sh

normalize()
{
  N=`echo "$1" | sed 's/-pc-/-/'`
  N=`echo "$N" | sed 's/-unknown-/-/'`
  echo "$N"
}

BUILD=`"${SCRIPTS:-.}/config.guess"`
COMP=`${CC:-cc} $CFLAGS -dumpmachine`
TARGET="${2:-${1:-$BUILD}}"

NTARGET=`normalize "$TARGET"`
NBUILD=`normalize "$BUILD"`
NCOMP=`normalize "$COMP"`

normalize_version()
{
  local TARGET="$1"
  local BUILD="$2"

  if echo "$BUILD" | grep -e '^i[1-9]86-' 2>&1 > /dev/null; then
    if echo "$TARGET" | grep -e '^i[1-9]86-' 2>&1 > /dev/null; then
      local CPU="${BUILD%%-*}"
      TARGET="${CPU}-${TARGET#*-}"
    fi
  fi

  if echo "$BUILD" | grep -e '-apple-darwin[0-9.]\+$'  > /dev/null; then
    if echo "$TARGET" | grep -e '-apple-darwin[0-9.]\+$'  > /dev/null; then
      local VERSION="${BUILD##*apple-darwin}"
      TARGET="${TARGET%apple-darwin*}apple-darwin${VERSION}"
    fi
  fi

  echo "$TARGET"
}

NTARGET=`normalize_version "$NTARGET" "$NBUILD"`
NCOMP=`normalize_version "$NCOMP" "$NBUILD"`

if [ "$NTARGET" = "$NCOMP" ]; then
  if [ "$NBUILD" != "$NCOMP" ]; then
    echo "Warning: Native compiler ($COMP) does not match build system ($BUILD)" >&2
    echo "--build=$TARGET --host=$TARGET"
  fi
else
  echo "Configuring with --host=$TARGET" >&2
  echo "--host=$TARGET"
fi

