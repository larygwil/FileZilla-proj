#! /bin/sh

if [ "$1" = "--abracadabra" ]; then
  shift
  ./configure "$@"
else
  # A little magic to unpack CONFIGURE_ARGS
  echo "$CONFIGURE_ARGS" | xargs "$0" --abracadabra "$@"
fi
