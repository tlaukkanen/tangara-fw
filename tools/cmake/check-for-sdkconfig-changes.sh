#!/bin/bash
SDKCONFIG=$PROJ_PATH/sdkconfig
SDKCONFIG_COMMON=$PROJ_PATH/sdkconfig.common
if [ ! -f "$SDKCONFIG" ]; then
  exit 0
fi
if [ "$SDKCONFIG" -nt "$SDKCONFIG_COMMON" ]; then
  exit 0
fi
RED='\033[0;31m'
NOCOLOUR='\033[0m'
echo -e "$RED########################################################################$NOCOLOUR"
echo "'sdkconfig.common' is newer than 'sdkconfig'! You may be building with"
echo "an out of date configuration. Delete your 'sdkconfig' to refresh your"
echo "build configuration, or 'touch sdkconfig' to silence this warning."
echo -e "$RED########################################################################$NOCOLOUR"
exit 1