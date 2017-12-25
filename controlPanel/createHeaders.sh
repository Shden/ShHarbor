#!/bin/bash
HEADERS_DIR=~/ShHarbor/controlPanel/headers

mkdir -p $HEADERS_DIR
rm $HEADERS_DIR/*.*
xxd -i index-compressed.html $HEADERS_DIR/index.html.h
xxd -i build/controlPanelApp.js.gz $HEADERS_DIR/controlPanelApp.js.gz.h
