#!/bin/bash
BUILD_DIR=~/ShHarbor/esp/ShHarbor/switch/.pioenvs/esp12
PUBLISH_DIR=~/Shden/shweb/firmware/ShHarbor/switch

cp ../../controlPanel/build/controlPanelApp.js.gz data/build/
cp ../../controlPanel/index.html data/

~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run -t buildfs

cp $BUILD_DIR/firmware.bin $PUBLISH_DIR/FW.bin
cp $BUILD_DIR/spiffs.bin $PUBLISH_DIR/SPIFFS.bin
cp $BUILD_DIR/../../data/version.info $PUBLISH_DIR/version.info
echo Done.
