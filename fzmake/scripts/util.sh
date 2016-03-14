#! /bin/sh

cpu_count()
{
  NPROC=`which nproc 2>/dev/null` 
  if [ ! -z "$NPROC" ] && [ -x "$NPROC" ]; then
    echo `$NPROC`
    return
  fi

  NPROC=`sysctl hw.ncpu 2>/dev/null | sed 's/.* //' 2>/dev/null`
  if [ ! -z "$NPROC" ]; then
    echo $NPROC
    return
  fi

  echo 1
}

relpath()
{
    # $1 and $2 may themselves be relative paths iff they exist
    source=$1
    target=$2
    [ -d "$source" ] && source=$(cd "$source"; pwd)
    [ -d "$target" ] && target=$(cd "$target"; pwd)

    common_part=$source
    result=

    while [ "${target#"$common_part"}" = "$target" ]; do
        # no match, means that candidate common part is not correct
        # go up one level (reduce common part)
        common_part=$(dirname "$common_part")
        # and record that we went back, with correct / handling
        if [ -z "$result" ]; then
            result=..
        else
            result=../$result
        fi
    done

    if [ "$common_part" = / ]; then
        # special case for root (no common path)
        result=$result/
    fi

    # since we now have identified the common part,
    # compute the non-common part
    forward_part=${target#"$common_part"}

    # and now stick all parts together
    if [ -n "$result" ] && [ -n "$forward_part" ]; then
        result=$result$forward_part
    elif [ -n "$forward_part" ]; then
        # extra slash removal
        result=${forward_part#?}
    else
        result="."
    fi

    echo "$result"
}
