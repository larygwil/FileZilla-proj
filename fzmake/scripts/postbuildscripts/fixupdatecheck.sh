#! /bin/sh

# This script updates the nightly information for the updatecheck script.

function fixupdatecheck()
{
  echo "Updating information for the automated update checks"
  
  LATEST="/var/www/nightlies/latest.php"
  WWWDIR="http://filezilla-project.org/nightlies"

  cd "$OUTPUTDIR"
  for TARGET in *; do
    if ! [ -f "$OUTPUTDIR/$TARGET/build.log" ]; then
      continue;
    fi
    if ! [ -f "$OUTPUTDIR/$TARGET/successful" ]; then
      continue;
    fi
    
    # Successful build
    
    cd "$OUTPUTDIR/$TARGET"
    for FILE in *; do
      if [ $FILE == "successful" ]; then
        continue;
      fi
      if [ $FILE == "build.log" ]; then
        continue;
      fi

      cat "$LATEST" | grep -v "$TARGET" | grep -v "?>" > "$LATEST.new"
      echo "\$nightlies['$TARGET'] = array();" >> $LATEST.new
      echo "\$nightlies['$TARGET']['date'] = '$DATE';" >> $LATEST.new
      echo "\$nightlies['$TARGET']['file'] = '$WWWDIR/$DATE/$TARGET/$FILE';" >> $LATEST.new
      echo "?>" >> $LATEST.new
      mv $LATEST.new $LATEST
    done
  done
}

fixupdatecheck

