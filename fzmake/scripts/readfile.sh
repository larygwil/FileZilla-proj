#! /bin/sh

load_file()
{
  if ! exec 3<$1; then
    echo "Failed to open '$1'"
    return 1
  fi

  return 0
}

read_file()
{
  while read -u3; do
    if [ "${REPLY:0:1}" = "#" ]; then
      continue
    fi

    if ! [ -z "$1" ]; then
      export $1="$REPLY"
    fi

    return 0
  done

  return 1
}

