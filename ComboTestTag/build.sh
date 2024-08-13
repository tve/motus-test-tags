#! /bin/bash -e
# Build the firsmware from scratch, prereq: arduino-cli installed
# Lots of stuff copied from https://github.com/adafruit/ci-arduino/blob/master/install.sh

export ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS=https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
set nonomatch=1

if ! which arduino-cli >/dev/null; then echo "Please install arduino-cli"; exit 1; fi

if ! [[ -d $HOME/.arduino15/packages/adafruit ]]; then
    arduino-cli core install adafruit:samd
fi

if ! [[ -f $HOME/.arduino15/library_index.json ]]; then
	arduino-cli lib update-index
fi
arduino-cli lib install RadioLib@6.6.0 CRC@1.0.3 "Arduino Low Power@1.2.2"

if ! [[ -x ../uf2conv/uf2conv ]]; then
    ( cd ../uf2conv; cc -o uf2conv uf2conv.c)
fi

for FREQ in 150.1 166.38; do
    arduino-cli compile --fqbn adafruit:samd:adafruit_feather_m0_express -e \
        --build-property "compiler.cpp.extra_flags=-DLTK_FREQ=$FREQ"

    NAME=${PWD##*/}
    ../uf2conv/uf2conv build/*/$NAME.ino.bin $NAME-${FREQ/./_}.uf2
done

if [[ -d /run/media/$USER/FEATHERBOOT ]]; then
    echo "Uploading"
    cp $NAME-166_38.uf2 /run/media/$USER/FEATHERBOOT
fi
