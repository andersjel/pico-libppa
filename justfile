set dotenv-load

# Configuration:
build_path := "build"
boot_loader := "/dev/disk/by-label/RPI-RP2"
tty := "/dev/ttyRP2040"

b := quote(build_path)

default: build

setup:
    git submodule update --init
    git -C vendor/pico-sdk submodule update --init

configure:
    mkdir -p {{b}}
    cd {{b}} && cmake ..

clean:
    rm -rf {{b}}

reconfigure: clean configure

build: configure
    make -C {{b}} -j`nproc`

install-udev-rules:
    sudo cp udev.rules /etc/udev/rules.d/72-pico.rules

_wait_for file:
    #!/bin/bash
    set -euo pipefail
    echo "Waiting for" {{quote(file)}}
    while [[ ! -e {{quote(file)}} ]]; do
        sleep 0.001
    done | pv -t > /dev/null

flash: build (_wait_for boot_loader)
    pv {{b}}/demo.uf2 > {{quote(boot_loader)}}

tio:
    tio {{quote(tty)}}

tio-no-reconnect: (_wait_for tty)
    tio -n {{quote(tty)}} ||:

run: flash tio-no-reconnect
