#!/bin/bash

if [ -d "build" ]; then
  rm -rf ./build
fi

mkdir build
make all
cp src/spy.ko build/tmp
make clean
mv build/tmp build/spy.ko
