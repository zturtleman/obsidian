#!/bin/sh

JOBS=2 # Number of make jobs to use, generally cpu cores x2 if you have hyperthreading.

# Build enet:
cd enet
./configure && make -j${jobs} || exit 1
cd ..

# Configure and compile obsidian:
./autogen.sh $@ && make -j${jobs} || exit 1
