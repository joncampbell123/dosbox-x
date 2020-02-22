#!/bin/sh

echo "Generating build information using aclocal, autoheader, automake and autoconf"
echo "This may take a while ..."

# If an error occurs, quit the script and inform the user. This ensures scripts
# like ./build-macosx etc. don't continue on if Autotools isn't installed.
function finish {
  echo 'autogen.sh failed to complete: verify that GNU Autotools is installed on the system and try again'
}
trap finish EXIT
set -e

# Regenerate configuration files.

aclocal
autoheader
automake --include-deps --add-missing --copy 
autoconf

echo "Now you are ready to run ./configure."
echo "You can also run  ./configure --help for extra features to enable/disable."
