alias b := build

build-init:
    cmake -S . -B build

build: 
    cmake --build build

server:
    ./build/src/server/server

client:
    ./build/src/client/client