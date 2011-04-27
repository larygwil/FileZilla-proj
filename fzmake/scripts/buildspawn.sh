#! /bin/sh

export SSH="ssh -o PreferredAuthentications=publickey -o StrictHostKeyChecking=yes -o BatchMode=yes -q"
export SCP="scp -o PreferredAuthentications=publickey -o StrictHostKeyChecking=yes -o BatchMode=yes -q -B"

failure()
{
  ENDSECONDS=`date '+%s'`
  local span=$((ENDSECONDS - STARTSECONDS))
  echo "Build time: $span seconds" >> "$TARGETLOG"
  touch "$OUTPUTDIR/$TARGET/failed"
  rm -f "$OUTPUTDIR/$TARGET/running"
  rm -f "$OUTPUTDIR/$TARGET/pending"
  rm -f "$WORKDIR/output-$TARGET.tar"
  rm -rf "$WORKDIR/$TARGET"
  return 1
}

all_failure()
{
  logprint "$TARGETS: Failed"
  for TARGET in $TARGETS; do
    TARGETLOG="$OUTPUTDIR/$TARGET/build.log"
    echo -e "FileZilla 3 build log" >> $TARGETLOG
    echo -e "---------------------\n" >> $TARGETLOG
    echo -e "Target: $TARGET" >> $TARGETLOG
    START=`date "+%Y-%m-%d %H:%M:%S"`
    echo -e "Build started: $START\n" >> $TARGETLOG

    echo -e "Failed to upload required files" >> $TARGETLOG
    
    touch "$OUTPUTDIR/$TARGET/failed"
    rm -f "$OUTPUTDIR/$TARGET/running"
    rm -f "$OUTPUTDIR/$TARGET/pending"
    rm -f "$WORKDIR/output-*.tar"
    rm -rf "$WORKDIR/$TARGET"
  done

  spawn_cleanup
  
  return 1
}

spawn_cleanup()
{
  if [ "$CLEANUP_DONE" = "true" ]; then
    return 1;
  fi 

  export CLEANUP_DONE=true
  logprint "$TARGETS: Performing cleanup"
  filter $SSH -i "$KEYFILE" -p $PORT "$HOST" ". /etc/profile; cd $HOSTPREFIX; rm -rf clientscripts work output output.tar clientscripts.tar.bz2;" || all_failure || return 1
}

buildspawn()
{
  ID=$1
  HOST=$2
  HOSTPREFIX=$3
  TARGETS=$4
  PACKAGES=$5

  CLEANUP_DONE=false

  PORT=${HOST#*:}
  if [ "$HOST" = "$PORT" ]; then
    PORT=22
  else
    HOST=${HOST%:*}
  fi 
  
  logprint "$TARGETS: Uploading packages"
  filter $SSH -i "$KEYFILE" -p $PORT "$HOST" ". /etc/profile; mkdir -p "$HOSTPREFIX/packages"; cd $HOSTPREFIX/packages && rsync -a --delete \"$UPDATESERVER\" ." 2>&1 || all_failure || return 1

  logprint "$TARGETS: Uploading clientscripts"
  filter $SCP -i "$KEYFILE" -P $PORT "$WORKDIR/clientscripts.tar.bz2" "$HOST:$HOSTPREFIX" || all_failure || return 1

  logprint "$TARGETS: Unpacking clientscripts"
  filter $SSH -i "$KEYFILE" -p $PORT "$HOST" ". /etc/profile; cd $HOSTPREFIX; rm -rf clientscripts; tar -xjf clientscripts.tar.bz2;" 2>&1 || all_failure || return 1

  logprint "$TARGETS: Building targets, check target specific logs for details"
  
  for i in $TARGETS; do
    export TARGET=$i
    export TARGETLOG="$OUTPUTDIR/$i/build.log"
    targetlogprint "FileZilla 3 build log"
    targetlogprint "---------------------\n"
    targetlogprint "Target: $TARGET";
    START=`date "+%Y-%m-%d %H:%M:%S"`
    export STARTSECONDS=`date '+%s'`
    targetlogprint "Build started: $START\n"
    
    touch "$OUTPUTDIR/$i/running"
    rm "$OUTPUTDIR/$i/pending"
    
    targetlogprint "Invoking remote build script"
    if [ -z "$SUBHOST" ]; then
      filter $SSH -i "$KEYFILE" -p $PORT "$HOST" ". /etc/profile; cd $HOSTPREFIX; clientscripts/build.sh \"$HOSTPREFIX\" \"$i\" \"$PACKAGES\"" >> $TARGETLOG || failure || continue
    else
      filter $SSH -i "$KEYFILE" -p $PORT "$HOST" "ssh $SUBHOST '. /etc/profile; cd $HOSTPREFIX; clientscripts/build.sh \"$HOSTPREFIX\" \"$i\" \"$PACKAGES\"'" >> $TARGETLOG || failure || continue
    fi

    targetlogprint "Downloading built files"
    filter $SCP -i "$KEYFILE" -P $PORT "$HOST:$HOSTPREFIX/output.tar" "$WORKDIR/output-$TARGET.tar" >> $TARGETLOG || failure || continue

    cd "$WORKDIR"
    tar -xf "$WORKDIR/output-$TARGET.tar" >> $TARGETLOG 2>&1 || failure || continue

    if [ ! -d "$TARGET" ]; then
      targetlogprint "Downloaded file does not contain target specific files"
      failure || continue
    fi

    cd "$TARGET"
    cp * "$OUTPUTDIR/$TARGET"

    cd "$WORKDIR"
    rm -r "$TARGET"
    rm "output-$TARGET.tar";

    touch "$OUTPUTDIR/$i/successful"
    rm "$OUTPUTDIR/$i/running"

    targetlogprint "Build successful"

    ENDSECONDS=`date '+%s'`
    local span=$((ENDSECONDS - STARTSECONDS))
    echo "Build time: $span seconds" >> "$TARGETLOG"

  done

  spawn_cleanup
}

