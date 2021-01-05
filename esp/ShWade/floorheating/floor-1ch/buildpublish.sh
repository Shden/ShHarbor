#!/bin/bash
# 1-channel floor thermostat build & publish script
# For house: Brod
BUILD_DIR=~/ShHarbor/esp/ShWade/floorheating/floor-1ch/.pio/build/esp12
PUBLISH_DIR=192.168.1.162:/home/den/Shden/shweb/ota/floorheating/floor-1ch

~/.platformio/penv/bin/pio run
~/.platformio/penv/bin/pio run -t buildfs

scp $BUILD_DIR/firmware.bin $PUBLISH_DIR/FW.bin
scp $BUILD_DIR/spiffs.bin $PUBLISH_DIR/SPIFFS.bin
scp $BUILD_DIR/../../../data/version.info $PUBLISH_DIR/version.info
echo Done.