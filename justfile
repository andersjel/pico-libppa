set dotenv-load

# Build path.
b := "build"

default: build

configure:
    mkdir -p {{b}}
    cd {{b}} && cmake ..

clean:
    rm -rf {{b}}

reconfigure: clean configure

build:
    make -C {{b}} -j`nproc`
