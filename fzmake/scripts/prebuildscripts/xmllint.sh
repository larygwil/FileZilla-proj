#! /bin/sh

# Prebuild script for nightlygen
# This function minifies XML files

echo "Minifying XML files"

cd "$WORKDIR/source/FileZilla3/src/interface/resources"
for i in defaultfilters.xml xrc/*.xrc theme.xml */theme.xml; do
  xmllint --noblanks "$i" > "$i~"
  mv "$i~" "$i"
done

