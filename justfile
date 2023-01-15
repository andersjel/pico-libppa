set dotenv-load

# Build path.
b := "build"

# Device file representing pico bootloader
device := "/dev/disk/by-label/RPI-RP2"
tty := "/dev/ttyRP2040"

default: build

configure:
    mkdir -p {{b}}
    cd {{b}} && cmake ..

clean:
    rm -rf {{b}}

reconfigure: clean configure

build: configure
    make -C {{b}} -j`nproc`

_wait_for file:
    #!/bin/bash
    set -euo pipefail
    echo "Waiting for" {{quote(file)}}
    while [[ ! -e {{quote(file)}} ]]; do
        sleep 0.001
        echo "."
    done | pv -t > /dev/null

flash: build (_wait_for device)
    pv {{b}}/demo.uf2 > {{device}}

tio:
    tio {{tty}}

tio-no-reconnect: (_wait_for tty)
    tio -n {{tty}} ||:

run: flash tio-no-reconnect
