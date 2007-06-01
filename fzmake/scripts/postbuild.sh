#! /bin/sh

postbuild()
{
  [ -f "$CONFDIR/postbuild" ] || return 0

  load_file "$CONFDIR/postbuild" || return 1

  while read_file; do
    
    if [ -z "$REPLY" ]; then continue; fi

    local script="$SCRIPTS/postbuildscripts/$REPLY"

    if ! [ -x "$script" ]; then
      echo "$script not found or not executable"
      continue
    fi

    . "$script"

  done
}

