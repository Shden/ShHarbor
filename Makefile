ARDMK_DIR = ~/Documents/Arduino/Arduino-Makefile/
CFLAGS_STD        = -std=gnu11
CXXFLAGS_STD      = -std=gnu++11

BOARD_TAG = nano
MCU = atmega328p
ISP_PORT = /dev/ttyACM*

include $(ARDMK_DIR)/Arduino.mk

# !!! Important. You have to use make ispload to upload when using ISP programmer
