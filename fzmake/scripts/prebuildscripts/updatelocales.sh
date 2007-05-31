#! /bin/sh

# Prebuild script for nightlygen

# This function updates the locales page

function updatelocales()
{
  local WWWLOCALES="/var/www/locales"

  echo "Updating locales page"
  
  cd "$WORKDIR/FileZilla3/locales"
  make >> $LOG 2>&1 || return 1

  logprint "Copying locales"

  # Copy pot template
  cp filezilla.pot $WWWLOCALES/filezilla.pot
  chmod 775 $WWWLOCALES/filezilla.pot

  rm -f $WWWLOCALES/stats~
  
  for i in *.po.new; do
  
    FILE=${i%%.*}
    PO=${i%.new}

    cp $i $WWWLOCALES/$PO
    chmod 775 $WWWLOCALES/$PO
    cp $FILE.mo $WWWLOCALES/
    chmod 775 $WWWLOCALES/$FILE.mo
    
    cp $i $i~

    cat >> $i~ << "EOF"

msgid "foobar1"
msgstr ""

msgid "foobar2"
msgstr "foobar2"

#, fuzzy
msgid "foobar3"
msgstr "foobar3"
EOF

    RES=`msgfmt $i~ -o /dev/null --statistics 2>&1`

    TR=$[`echo $RES | sed -e 's/^[^0-9]*\([0-9]\+\)[^0-9]\+\([0-9]\+\)[^0-9]\+\([0-9]\+\)[^0-9]\+/\1/'` - 1]
    FZ=$[`echo $RES | sed -e 's/^[^0-9]*\([0-9]\+\)[^0-9]\+\([0-9]\+\)[^0-9]\+\([0-9]\+\)[^0-9]\+/\2/'` - 1]
    UT=$[`echo $RES | sed -e 's/^[^0-9]*\([0-9]\+\)[^0-9]\+\([0-9]\+\)[^0-9]\+\([0-9]\+\)[^0-9]\+/\3/'` - 1]

    rm $i~

    LANG=${FILE%_*}
    COUNTRY=${FILE#*_}

    LANGNAME=`cat "$DATADIR/languagecodes" 2>/dev/null | grep "^$LANG "`
    if [ $? ]; then
      LANGNAME=${LANGNAME#* }
    else
      LANGNAME=
    fi

    COUNTRYNAME=
    if [ ! -z "$COUNTRY" ]; then
      COUNTRYNAME=`cat "$DATADIR/countrycodes" 2>/dev/null | grep "^$COUNTRY "`
      if [ $? ]; then
        COUNTRYNAME=${COUNTRYNAME#* }
      fi
    fi

    NAME=
    if [ ! -z "$LANGNAME" ]; then
      NAME="$LANGNAME " 
    fi
     
    if [ ! -z "$COUNTRYNAME" ]; then
      NAME="$NAME($COUNTRYNAME) " 
    fi

    echo "$FILE $TR $FZ $UT $NAME" >> $WWWLOCALES/stats~
    
  done

  chmod 775 $WWWLOCALES/stats~
  mv $WWWLOCALES/stats~ $WWWLOCALES/stats
}

updatelocales
