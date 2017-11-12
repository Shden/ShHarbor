#!/bin/bash
BUILD_DIR=~/ShHarbor/switch/.pioenvs/esp12
PUBLISH_DIR=~/Shden/shweb/firmware/ShHarbor/switch
BUILD_NUMBER=$(<.buildnumber)

cp $BUILD_DIR/firmware.bin $PUBLISH_DIR/SHH-SW-$BUILD_NUMBER.bin
rm $PUBLISH_DIR/version.info
echo $BUILD_NUMBER > $PUBLISH_DIR/version.info
echo ShHarbor Switch version $BUILD_NUMBER published to $PUBLISH_DIR.

BUILD_NUMBER=$(($BUILD_NUMBER + 1))
rm .buildnumber
echo $BUILD_NUMBER > .buildnumber
echo Next build will have number $BUILD_NUMBER.
