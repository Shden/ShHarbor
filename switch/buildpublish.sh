#!/bin/bash
BUILD_DIR=~/ShHarbor/switch/.pioenvs/esp12
PUBLISH_DIR=~/Shden/shweb/firmware/ShHarbor/switch

~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run -t buildfs

cp $BUILD_DIR/firmware.bin $PUBLISH_DIR/SHH-SW-FW.bin
cp $BUILD_DIR/spiffs.bin $PUBLISH_DIR/SHH-SW-SPIFFS.bin
cp $BUILD_DIR/../../data/version.info $PUBLISH_DIR/version.info
echo Done.
