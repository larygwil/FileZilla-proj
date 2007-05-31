#! /bin/sh

prebuild()
{
  [ -f "$CONFDIR/prebuild" ] || return 0

  load_file "$CONFDIR/prebuild" || return 1

  while read_file; do
    
    if [ -z "$REPLY" ]; then continue; fi

    local script="$SCRIPTS/prebuildscripts/$REPLY"

    if ! [ -x "$script" ]; then
      echo "$script not found or not executable"
      continue
    fi

    . "$script" || return 1

  done
}

